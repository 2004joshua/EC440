# EC440: Shell Parser

The sample code provided for the Parser includes:
- [Makefile](Makefile):  The Makefile that builds your parser, tests, and runs your test programs as described [here](https://openosorg.github.io/openos/textbook/intro/tools-make.html#a-simple-example)
- [myshell_parser.h](myshell_parser.h): The header file that describes the structures and interfaces that clients of your parser needs to know (see [this](https://openosorg.github.io/openos/textbook/intro/tools-testing.html#testing)).
- [myshell_parser.c](myshell_parser.c): An initial stub implementation of the functions that you will need to fill in.
- [run_tests.sh](run_tests.sh): A shell script, described [here](https://openosorg.github.io/openos/textbook/intro/tools-shell.html#shell) for automating running all the tests

Two example test programs that exercise the interface of the parser, discussed [here](https://openosorg.github.io/openos/textbook/intro/tools-testing.html#testing), are:
- [test_simple_input.c](test_simple_input.c): Tests parsing the command 'ls'
- [test_simple_pipe.c](test_simple_pipe.c): Tests parsing the command 'ls | cat`

Please modify this README.md file to describe any interesting strategy you used to develop the parser and list any references to resources you used to develop your solution. Also, please add a line above for each test program you develop. 

Submission notes: 

First off I just want to start off by saying this has been the hardest coding assignment I've received and had many doubts of me taking this class as early as I am. Luckily I've managed to hold and was able to complete the assignment. 

First part of this assignment was making the parser for the shell

  -For the first week of classes I've spent learning ways I can parse the command line and I originally wanted to go through each line since it seemed like the most logical way of parsing a commandline. 

  -It was also my first week using emacs and using it was extremely frustrating since I wasnt used to using it so I ended up using vim instead and I tried going back and forth with vim and emacs but ultimately I decided vim was more for me. 

  -That whole week I also spent learning what the necessary functions do and I watched all the videos a youtuber by the name of code vault and it taught me the proper way of using functions such as fork() and the exec functions. 

  -Back to the parsing I kept confusing myself with the logic of parsing and my original code I utilized a counter to keep track of the input word I was making but again it just made my head hurt by keeping up with that counter, after failure after failure to parse I was recommended by one of the TAs that one valid way was using the strtok function to tokenize the commandline.

  -Lets just say that I found much success using strtok and was able to pass the original two test cases we were given. I felt so happy that it actually worked and passed it in gradescope. It failed all but one case and I was essentially back to square one. I utilized chatgpt to check if there was any flaws with my logic and it ended up being useless since it couldnt give me a proper fix to this problem. 

  -out of options, I then remembered you mentioned if you wanted to be successful in this class you had to learn how to properly utilize a debugger and thats what I did. I quickly went back to my ec327 labs where we briefly went over how to use the gdb debugger. 

  -There I saw how strtok split up the input command line and saw many inconsistencies in how it split up the command line, at least when there was no spaces between a special token and a command/argument

  -my first thought to resolve this problem was looping through each index of the token to check if they had a special char or not, if not I would place it as a command or argument, when I tried implementing it I found this approach as stupid and deleted all my code once again.

  -with a fresh start I decided to give the individual index a try again and I used a variable to keep track of the index of the input word, the command_arg, and the full command line

  -once again I found myself getting lost with the logic of the index of the input word so I checked geeksforgeeks and saw that i can just append letters to a word so whenever I reached a char that wasnt a space newline or tab I would just append it using a temp variable making it temp[2] = {copy_commandline[i], '\0'} 

  -now that the parsing itself was working properly, I had many problems with properly redirecting in or out. The biggest problem was that it was putting the redirection file inside the command_args array and I knew I didnt want that. I checked geeksforgeeks and saw that I can use typedef enum to create states my code can be in so obviously I made 3 to reflect if I have passed one of the redirection tokens. 

  -anything before a redirection token would have state = NORMAL and only the input after would have state = REDIRECT_IN or state = REDIRECT_OUT. and from that I was able to finish off the redirects. 

  -the pipe token was pretty straight forward to be honest it was just making a new node in the linked list and makeing new_current->next = NULL so it can be the last one. same with the background it was just making pipeline->is_background = true; 

  -also for each case I made it so that incase word_input wasnt empty I would just add it to command_args, same with space newline and tab. 

  -the last problems I had was when I asked you to help me debug the code after class. after using gdb you helped me realize that I wasnt making the next comamnd_arg index end with null and it was messing up at the autograder. because of it. 

  -Even after I still had some small bugs with the debugger but I was able to fix those on the due date. 

Finally im at the shell itself: 

  -Initially I didn't know how to begin so I went back to those youtube videos by code vault and saw examples of how he used the necessary functions
  
  -My first step was making one big while loop that would go on forever since thats how the terminal itself begins, doesnt end until you close it manually. 
  
  -I initialized my input word, mypipeline using the pipeline build function, and a current variable that points to the commands within the pipeline
  
  -from there I was confused on how to put my knowledge from the videos into practice, so I read a paper from purdue (https://www.cs.purdue.edu/homes/grr/SystemsProgrammingBook/Book/Chapter5-WritingYourOwnShell.pdf) and this paper at https://tldp.org/LDP/lpg/node11.html#:~:text=To%20create%20a%20simple%20pipe,be%20used%20for%20the%20pipeline., I saw the basic templates on how to deal with children and was able to properly implement it 
  
  -from there building the shell itself was pretty straight forward, using dup2 for the redirection files and for redirecting in and out of pipes and using execvp to execute the command_args array and the whole thing was rinse and repeat

  -I believe the purpose of this project was to give us an introduction to how operating systems work by interacting with one of the most important component being the kernel and learning more under the hood components we wouldnt learn in our intro to software eng class

  



