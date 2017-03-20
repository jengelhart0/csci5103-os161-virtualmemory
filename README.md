
# csci5103os161
OS161 project for Operating Systems class  

https://ay16.moodle.umn.edu/pluginfile.php/1883744/mod_resource/content/2/PA0.pdf  

## 2.4 Congure, Build, and Run OS/161   
A detailed explanation on how to congure, build, and run the OS can be found in this link:  http://os161.eecs.harvard.edu/resources/building.html.

* module load os161
* module unload os161
* module initadd os161


1. Congure the source tree:
  * cd ~/os161/src
  * ./congure --ostree=$HOME/os161/root

2. Build userland:
  * bmake
  * bmake install  

3. Congure a kernel:
  * cd kern/conf;
  * ./cong DUMBVM (assuming DUMBVM is the conguration that you are using)

4. Compile and install the kernel:
  * cd ~/os161/src/kern/compile/DUMBVM
  * bmake depend
  * bmake
  * bmake install

5. Copy a sample conguration file from /project/s17c5103/os161/sys161.conf to the ~/os161/root/ directory and run your kernel:
  * cd ~/os161/root
  * sys161 kernel
