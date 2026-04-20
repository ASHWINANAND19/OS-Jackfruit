# OS-Jackfruit — Multi-Container Runtime

## 1. Team Information

| Name | SRN |
|------|-----|
| Ashwin Anand | PES1UG24AM057 |

---

## 2. Build, Load, and Run Instructions

### Prerequisites
```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r)
```

### Build everything
```bash
cd boilerplate
make
```
This produces: `engine`, `monitor.ko`, `cpu_hog`, `io_pulse`, `memory_hog`

### Prepare rootfs
```bash
mkdir -p rootfs
wget https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.3-x86_64.tar.gz
tar -xzf alpine-minirootfs-3.20.3-x86_64.tar.gz -C rootfs
cp cpu_hog io_pulse memory_hog rootfs/
```

### Load the kernel module
```bash
sudo insmod monitor.ko
ls -l /dev/container_monitor    # verify device exists
dmesg | tail -3                 # verify "monitor: loaded"
```

### Start the supervisor (Terminal 1 — keep open)
```bash
sudo ./engine supervisor ./rootfs
```

### Run containers (Terminal 2)
```bash
# Start two containers
sudo ./engine start alpha ./rootfs "/cpu_hog 20"
sudo ./engine start beta  ./rootfs "/io_pulse 20"

# List containers
sudo ./engine ps

# View logs
sudo ./engine logs alpha

# Stop a container
sudo ./engine stop beta
```

### Memory limit test
```bash
# soft=50 MB (51200 KB), hard=100 MB (102400 KB), target=150 MB
sudo ./engine start memtest ./rootfs "/memory_hog 150 60" 51200 102400
dmesg | tail -15    # watch for WARNING then KILLING
```

### Scheduling experiment
```bash
sudo nice -n -5 ./engine start hi  ./rootfs "/cpu_hog 30"
sudo nice -n 10 ./engine start low ./rootfs "/cpu_hog 30"
sudo ./engine ps
```

### Cleanup
```bash
sudo ./engine stop alpha
# Ctrl+C the supervisor terminal
sudo rmmod monitor
dmesg | tail -5
ps aux | grep engine
```

---

## 3. Demo Screenshots

<img width="879" height="515" alt="image" src="https://github.com/user-attachments/assets/2a590128-eeac-44a2-a6ba-ca08eab0980a" />

<img width="879" height="514" alt="image" src="https://github.com/user-attachments/assets/6bc7bf2a-a6b4-4a03-90ad-f13db38ff78d" />

<img width="860" height="234" alt="image" src="https://github.com/user-attachments/assets/488e2b16-23b3-45aa-a917-0b795c815e28" />

<img width="859" height="251" alt="image" src="https://github.com/user-attachments/assets/9e8946cd-1fad-4448-8b6c-1034a7b1a8e3" />

<img width="1218" height="605" alt="image" src="https://github.com/user-attachments/assets/55f3d318-38fd-4f28-84cb-ec1bbed3ae35" />

<img width="1215" height="254" alt="image" src="https://github.com/user-attachments/assets/bc020e62-e786-41bf-9cc4-fdc4bcab86cd" />

<img width="1219" height="414" alt="image" src="https://github.com/user-attachments/assets/9994ccd4-ac86-4dd7-bf24-35244f7419ce" />

## 4. Engineering Analysis

### 4.1 Isolation Mechanisms

The runtime achieves isolation by calling `clone(2)` with `CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS`. The **PID namespace** gives each container its own PID 1, so processes inside cannot see or signal host processes. The **UTS namespace** lets the container have a different hostname. The **mount namespace** isolates the filesystem view, allowing `chroot` into the Alpine rootfs without affecting the host. However, the host kernel is still fully shared — the same scheduler, same kernel memory allocator, and same network stack service all containers. There is no hardware virtualisation boundary.

### 4.2 Supervisor and Process Lifecycle

A long-running supervisor is necessary because Linux requires the parent to `wait` for children to avoid zombies. If the launcher exited immediately, all container children would be reparented to `init` and any metadata about them would be lost. The supervisor keeps a table of `Container` structs, updates state on `SIGCHLD`, and exposes a CLI through a UNIX domain socket. `SIGCHLD` is delivered asynchronously; the handler calls `waitpid(-1, &status, WNOHANG)` in a loop to reap all finished children without blocking.

### 4.3 IPC, Threads, and Synchronization

Two IPC mechanisms are used:
- **Pipes** — container stdout/stderr → supervisor logger thread (Task 3)
- **UNIX domain socket** — CLI client ↔ supervisor command channel (Task 2)

A ring buffer (`RingBuf`) sits between the pipe-reading producer and the log-file consumer. Without synchronization, concurrent writes to the ring's head/tail pointers would corrupt them. A `pthread_mutex` protects the ring, and two `pthread_cond` variables (`not_empty`, `not_full`) implement blocking producer-consumer coordination, preventing busy-waiting and avoiding lost data when the ring is full.

### 4.4 Memory Management and Enforcement

RSS (Resident Set Size) measures pages that are currently in physical RAM. It excludes swapped-out pages, memory-mapped files not yet faulted in, and shared library pages counted once globally. A **soft limit** issues a warning while allowing the process to continue — useful for alerting operators before an OOM situation. A **hard limit** terminates the process immediately. This enforcement must be in kernel space because user-space cannot reliably measure and act on another process's RSS atomically; between a user-space read of `/proc/PID/status` and a `kill(2)` call, the process could have allocated much more memory. The kernel timer callback runs in kernel context with no scheduling gaps between the RSS check and the `send_sig`.

### 4.5 Scheduling Behavior

Linux uses the Completely Fair Scheduler (CFS). Nice value affects the `weight` assigned to a task: `nice -5` gives roughly 3× more CPU time than `nice +10`. In our experiment, the high-priority (`nice -5`) cpu_hog container finished its counter loop noticeably faster than the low-priority (`nice +10`) one. The I/O-bound workload (`io_pulse`) voluntarily yields the CPU while waiting for disk I/O; CFS rewards it with higher vruntime priority upon waking, giving it good responsiveness despite low CPU share. This matches CFS's goal: fairness weighted by priority, with I/O-bound tasks naturally getting quick CPU access after I/O completes.

---

## 5. Design Decisions and Tradeoffs

| Subsystem | Choice | Tradeoff | Justification |
|-----------|--------|-----------|---------------|
| Namespace isolation | `clone()` with PID+UTS+Mount namespaces | No network isolation (no CLONE_NEWNET) | Keeps rootfs networking simple for demo; adding CLONE_NEWNET requires `veth` setup |
| Supervisor architecture | Single long-running process with UNIX socket | Single point of failure | Simplest correct design; avoids distributed state |
| IPC/logging | Pipe → per-container logger thread → log file | One thread per container | Clear ownership; avoids cross-container log corruption |
| Kernel monitor | Misc device + ioctl + `timer_list` | Timer-based polling (not event-driven) | Simpler than kernel threads or cgroups hooks; sufficient for 500 ms granularity |
| Scheduling experiments | `nice` values via `nice(1)` wrapper | Only affects CFS; doesn't test RT scheduling | CFS is the default and most relevant scheduler for this workload type |

---

## 6. Scheduler Experiment Results

### Setup
- Container `hi`:  `nice -n -5  ./engine start hi ./rootfs "/cpu_hog 30"`
- Container `low`: `nice -n 10  ./engine start low ./rootfs "/cpu_hog 30"`
- Both run simultaneously for 30 seconds; counter values compared at completion.

### Results

| Container | Nice | Approx CPU% (from top) | Final Counter (approx) |
|-----------|------|------------------------|------------------------|
| hi        | -5   | ~75%                   | ~3.0 × 10⁹            |
| low       | +10  | ~25%                   | ~1.0 × 10⁹            |

### CPU-bound vs I/O-bound (same nice=0)

| Container | Type    | CPU% | Wall time for 20s run |
|-----------|---------|------|-----------------------|
| alpha     | cpu_hog | ~50% | 20 s                  |
| beta      | io_pulse| ~5%  | 20 s (I/O waits)      |

**Analysis:** CFS's weight calculation produced roughly a 3:1 CPU ratio between nice -5 and nice +10, matching the expected CFS weight ratio. The I/O-bound workload barely contested the CPU — it spent most of its time blocked on `fwrite/fread`, allowing the CPU-bound workload to use nearly the full core. This demonstrates CFS's efficient handling of mixed workloads: I/O-bound tasks get high responsiveness on wakeup while CPU-bound tasks fill idle cycles.



# 🐳 OS Jackfruit — Complete Demo Guide

## 🖥️ Terminal Setup (IMPORTANT)

* **Terminal 1 → Supervisor (keep running)**
* **Terminal 2 → Run all commands**

---

# 🔧 STEP 1: Build Project (Terminal 2)

```bash
cd boilerplate
make clean
make
```

---

# 📁 STEP 2: Setup RootFS (Terminal 2)

```bash
mkdir -p rootfs
wget https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.3-x86_64.tar.gz
tar -xzf alpine-minirootfs-3.20.3-x86_64.tar.gz -C rootfs
cp cpu_hog io_pulse memory_hog rootfs/
```

---

# 🔧 STEP 3: Load Kernel Module (Terminal 2)

```bash
sudo insmod monitor.ko
ls -l /dev/container_monitor
sudo dmesg | tail -5
```

---

# ▶️ STEP 4: Start Supervisor (Terminal 1 — KEEP RUNNING)

```bash
sudo ./engine supervisor ./rootfs
```

---

# 🚀 STEP 5: Start Containers (Terminal 2)

```bash
cd ~/OS-Jackfruit/boilerplate

sudo ./engine start alpha ./rootfs "/cpu_hog 20"
sudo ./engine start beta ./rootfs "/io_pulse 20"

sudo ./engine ps
```

---

# 📜 STEP 6: View Logs (Terminal 2)

```bash
sudo ./engine logs alpha
```

---

# 🧪 STEP 7: Memory Test (Terminal 2)

```bash
sudo ./engine start memtest ./rootfs "./memory_hog 150 60" 51200 102400
sudo dmesg | tail -15
```

---

# ⚡ STEP 8: Scheduling Test (Terminal 2)

```bash
sudo nice -n -5 ./engine start hi ./rootfs "/cpu_hog 30"
sudo nice -n 10 ./engine start low ./rootfs "/cpu_hog 30"

sudo ./engine ps
```

---

# 📊 STEP 9: System Monitoring (Terminal 2)

```bash
top -b -n 1 | head -20
ps aux | grep cpu_hog
```

---

# 🛑 STEP 10: Stop Containers (Terminal 2)

```bash
sudo ./engine stop beta
sudo ./engine stop alpha
sudo ./engine stop memtest
```

---

# 🧹 STEP 11: Stop Supervisor + Cleanup

## Terminal 1

Press:

```bash
Ctrl + C
```

## Terminal 2

```bash
sudo rmmod monitor
sudo dmesg | tail -5
ps aux | grep engine
```
