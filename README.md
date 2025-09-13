# CMPT 201 Coursework

This repository contains my work for **CMPT 201** at SFU.  
It includes labs, assignments, and notes completed inside the course Docker dev environment.

---


## Getting Started

### 1. Clone the repo
```bash
git clone https://github.com/<your-username>/cmpt201-coursework.git
cd cmpt201-coursework
```

### 2. Run inside the Docker environment
```bash
docker start -ai cmpt201
```

### 3. Compile and run code
Example for `lab1.c`:
```bash
cd lab1
clang lab1.c -fsanitize=address -o lab1
./lab1
```

---

## Notes
- All labs are built and tested inside the official CMPT-201 Docker container.  
- Submissions are made through CourSys.  
- Code is written in C and compiled with `clang`.

---

## Course Info
- **Course**: CMPT 201
- **School**: Simon Fraser University  
- **Language**: C (with POSIX extensions)  
