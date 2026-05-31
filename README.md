# Fuzzy-Files-Search-for-Linux-Android
Fuzzy File Search written in C

<br><br>
Compile on Android (Termux) with:
<br><br>
clang --target=aarch64-linux-android -Oz -flto fast_search_android.c -o fast_search_android
<br><br>
or
<br><br>
clang -O3 -march=native -flto -ffast-math fast_search_android.c -o fast_search_android

<br><br>
Compile on Linux with:
<br><br>
clang -03 fast_search.c -o fast_search

<br><br>
<img width="1080" height="2400" alt="1001291618" src="https://github.com/user-attachments/assets/53c09edf-c973-42b5-b032-2f5a8fa79cff" />
