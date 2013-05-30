COMPILING
=========

The steps listed below are necessary to compile the server.  
All commands must be executed in the root directory of the project unless otherwise noted,

If you use Windows, then you have to install [Cygwin](http://www.cygwin.com/) to perform these steps.  
You also need [Cygwin Ports](http://sourceware.org/cygwinports/) because the required package `liblog4cxx-dev` is not included in Cygwin.

1. Make sure the following packages are installed:
   - autoconf (2.69 or newer)
   - automake (1.11 or newer)
   - bash-completion
   - g++ (4.7.2 or newer)
   - libboost1.50-all-dev (or newer)
   - libgmp-dev
   - liblog4cxx10-dev
   - liglua5.1-dev
   - libmysqlclient-dev
   - libxml2-dev
   - libz-dev
   - make
   
   e.g. `apt-get install autoconf automake bash-completion g++ libboost1.50-all-dev libgmp-dev liblog4cxx10-dev liblua5.1-dev libmysqlclient-dev libxml2-dev libz-dev make`
   
   If you use Windows then you have to use Cygwin's `setup.exe` to install these packages.
   
2. Prepare the project using `./prepare.sh`.  
   You can pass additional arguments which will be passed to `configure`.
   
3. Compile the project using `./make.sh`.  
   You can pass additional arguments which will be passed to `make`, e.h. `-j3` to use three cores in parallel for compilation.
   
4. You are done!  
   The output file is located at `.intermediate/server`.


You can remove all output and intermediate files to start clean using `./clean.sh`.   
To simply clean intermediate compilation files use `./make.sh clean`.


Common Problems
---------------

- **`./prepare.sh` fails with `configure: error: *** A compiler with support for C++11 language features is required.`**  
    
  If you use Linux then your distribution may be too old and does not include a recent version of g++ (4.7.2 or newer).  
  Upgrade your distribution and your packages.
  
  If you use Windows then you may have to explicitly tell Cygwin to install the 4.7+ version of the g++ package.
