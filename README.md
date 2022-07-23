Compilation Instructions:

Compile: gcc -o mfs src/mfs.c
To Run: ./mfs

About This Project:
- This Program is able to open and read a FAT32 image. There are a variety of commands recognized by the program that allow you to obtain information about files within the FAT32 image, along with pulling the files into your local directory. 

Command List:
open "FAT32.img name" ->opens the FAT32 image

close -> closes the FAT32 image

info -> gives information about the FAT32 image (bytes per cluster, number of FATs, etc.)

stat "file name in image" (ex. stat foo.txt) -> gives information about the file 

get "file name in image" (ex. get foo.txt) -> pulls the file from the image to be saved in the directory the program is ran

cd "folder name" -> move pointer to folder head

ls -> lists all files within current directory

read "file name" "integer for starting postion" "integer for number bytes" -> gives hex values for the file for number bytes requested

If you enjoy this project, take a look at some of my other repositories and bookmark my personal site: https://jaedyn-portfolio.netlify.app



