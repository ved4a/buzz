# TaskMan: A Lightweight System Resource Monitor
## How 2 Run (4 my own reference)
**Note:** working dir is project-1-another1bytesthedust
### Backend
```
    cd backend
    mkdir build && cd build
    cmake ..
    make
    ./system_monitor
```
The output should be pretty JSON.
### Frontend

```
pip install pyqt5 psutil
cd frontend
python3 main.py
```
A box should pop up with a button saying "Get Metrics" upon which CPU and RAM usage will be pulled from our backend.
## File Structure (Current)
```
> backend
    > build
        build files...
    > include
        > nlohmann
            json.hpp
        battery.hpp
        cpu.hpp
        memory.hpp
        processes.hpp
        network.hpp
    > src
        main.cpp
        battery.cpp
        cpu.cpp
        disk.cpp
        memory.cpp
        network.cpp
        processes.cpp
    CMakeLists.txt
> dist
    final executable
> frontend
    main.py
    > assets
        ...
    ...
.gitignore
README.md
requirements.txt
```

## File Structure (Future)
```
> backend
    > build
        build files...
    > include
        > nlohmann
            json.hpp
        cpu.hpp
        memory.hpp
        process.hpp
        disk.hpp
        network.hpp
    > src
        main.cpp
        cpu.cpp
        memory.cpp
        process.cpp
        disk.cpp
        network.cpp
    CMakeLists.txt
> dist
    final executable
> frontend
    main.py
.gitignore
README.md
requirements.txt
```

[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/ybo2Cjmz)