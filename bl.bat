chcp 65001
windres -c 65001 Project1.rc Project1.o && copy eth-lib\eth-lib.dll . && del eth-lib\eth-lib.dll && g++ -shared -static -std=c++20 -pedantic -g -O0 -Wall -municode -m64 -o main.dll main.cpp eth-lib.dll -lgdi32 -lopengl32
