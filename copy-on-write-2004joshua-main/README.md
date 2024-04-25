[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-24ddc0f5d75046c5622901739e7c5dd533143b0c8e959d652212380cedb1ea36.svg)](https://classroom.github.com/a/QXoi_fo8)

The purpose of this project is to explore fundamental concepts of memory management in computing. We'll be creating a program that allows each thread, which is like a mini-task, to have its own private memory area. This is important because normally, all threads share the same memory, which can cause problems if one thread changes something that another thread was expecting to stay the same.

The cool part is implementing something called "copy-on-write" (COW). This means that when a thread wants to change something in its memory, it makes a copy just for itself, so the changes don't affect other threads. It's like having a shared notebook where, if you want to edit a page, you photocopy it and make your changes on the copy, leaving the original page untouched for others.

By working on this project, you'll get a deeper understanding of how computer programs use and manage memory, and how to keep different parts of a program from stepping on each other's toes when they're working with the same data.

Sources Used:
OS-MM slides: https://piazza.com/class_profile/get_resource/lqnz3iw9y02fc/ltxcacrigbk66v
OpenBSD manual page server: https://man.openbsd.org/tls_read.3
Atomic variables: https://youtu.be/_xX25ThomIo?si=TWFoJzZI5XSZbEnc
