#
# This file is executed by the makefile in ../
#

#copy index.html to /docs folder and change the base href to point to "build/html/"

def installIndexHtml(src, dst):
    task = "installing index html"
    print(task + "...", end="")
    try:
        with open(src, "r") as r, open(dst, "w") as w:
            w.write(r.read().replace('<head>','<head><base href="build/html/">'))

        print(" done")
    except Exception as e:
        print(" failed: " + str(e))

def installIndexHtmlLegacy(src, dst):
    task = "installing index html"
    print(task + "...", end="")
    try:
        #read string from file
        with open(src, "r") as r:
            str = r.read()

        #add base href tag
        str = str.replace('<head>','<head><base href="build/html/">')

        #write back
        with open(dst, "w") as w:
            w.write(str)

        print(" done")
    except Exception as e:
        print(" failed: " + str(e))


installIndexHtml("build/html/index.html", "index.html")