# oracolo_fightlist
C++ semi-automatic tool used to demonstrate our knowledge (demolish opponents) in FightList, a quite well known phone app. This tool is based on image processing (OpenCV library) and text recognition (TesseractOCR).  Only works on Windows PC and the emulator must be BlueStacks.

## General Info
The program asks for the language, then automatically recognises the topic, then asks if the guess is correct (could fail sometimes, in which case you can answer "n" and write the right name yourself).
Then it will search for the local file containing the answers (if exists, solutions come from a random website and most of the time they exist and are accurate) and then will proceed
to write the answers. There are some topics with 1700+ correct answers if the game nationality is set to US, so the program will automatically spend game coins to gain extra time (45 seconds
aren't enough even at a rate of 30 words per second). Since the program is based on image processing and text recognition there is no way for it for sending inputs other than directly controlling mouse and keyboard. 

Important advice based on experience: bluestacks appears to be randomly freezing because of the high rate of keyboard events. Setting the emulator to run in single core drastically reduced the issue on my machine, it could also work for you.

## Possible Problems Info
There are some tweaks to do in order to getting this program to work, it is unlikely to work out of the box because i didn't do it thinking of releasing it to the public in the first place.
If the program tells that no window named bluestacks is found (or something like that) then you need to check the exact name
of the window emulating FightList and insert it in the source code in either the first or the second window name variable (be careful since it is case sensitive).
You will need OpenCV and Tesseract installed to be able to build the source code.
You will also need an additional library for json file reading/writing https://github.com/nlohmann/json

