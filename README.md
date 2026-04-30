عشان حضرتك تعمل build تمشي ع الخطوات دي
1 - افتح ال cmd as admin 
تشيك لو انت محمل MINGW ( لازم و اساسي ) عن طريق الامر "gcc --version"
2 - run this command "g++ main.cpp gui.cpp scheduler.cpp -o scheduler_rr_srtf.exe -municode -mwindows -lgdi32 -lcomctl32 -luser32"
