#
# This file is executed by the makefile in ../
#

#copy index.html to /docs folder and change the base href to point to "build/html/"
from shutil import copyfile
import fileinput

def installIndexHtml(src, dst):
    task = "installing index html"
    print(task + "...", end="")
    try:
        #copyfile(src, dst) 

        #read string from file
        with open(src, "r") as r:
            str = r.read()
        
        #rfile = open(dst, "r")
        #str = rfile.read()
        #rfile.close()

        #add base href tag
        str = str.replace('<head>','<head><base href="build/html/">')

        #write back
        with open(dst, "w") as w:
            w.write(str)

        print(" done")
    except Exception as e:
        print(" failed: " + str(e))

installIndexHtml("build/html/index.html", "index.html")