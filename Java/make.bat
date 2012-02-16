%~d1
cd %~p1
javac %~n1%~x1
javah -jni %~n1
copy %~n1.class .\..\..\..\
pause
