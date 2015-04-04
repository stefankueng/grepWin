/* This script is a local pre-commit hook script.
 * It's used to check whether the copyright year of modified files has been
 * bumped up to the current (2015) year.
 *
 * Only *.cpp, *.h and *.idl files are checked
 *
 * Set the local hook scripts like this (pre-commit hook):
 * WScript path/to/this/script/file.js
 * and set "Wait for the script to finish"
 */

var ForReading = 1;
var objArgs;
var num;
objArgs = WScript.Arguments;
num = objArgs.length;
if (num !== 4 && num !== 3)
{
    WScript.Echo("Usage: [CScript | WScript] checkyear.js path/to/pathsfile depth path/to/messagefile path/to/CWD");
    WScript.Quit(1);
}

// readFileLines
function readPaths(path)
{
    var retPaths = [];
    var fileSystem = new ActiveXObject("Scripting.FileSystemObject");
    if (fileSystem.FileExists(path))
    {
        var textFile = fileSystem.OpenTextFile(path, ForReading);
        while (!textFile.AtEndOfStream)
        {
            var line = textFile.ReadLine();
            retPaths.push(line);
        }
        textFile.Close();
    }
    return retPaths;
}

var found = true;
var files = readPaths(objArgs(0));
var fileindex = files.length;
var errormsg = "";

while (fileindex--)
{
    var f = files[fileindex];
    var fs = new ActiveXObject("Scripting.FileSystemObject");
    var re = /^\/\/ Copyright.+(2015)(.*)/;
    var basere = /^\/\/ Copyright(.*)/;
    var filere = /(\.cpp$)|(\.h$)|(\.idl$)/;

    if (f.match(filere) !== null)
    {
        if (fs.FileExists(f))
        {
            var a = fs.OpenTextFile(f, ForReading, false);
            var copyrightFound = false;
            var yearFound = false;
            while (!a.AtEndOfStream && !yearFound)
            {
                var r = a.ReadLine();
                var rv = r.match(basere);
                if (rv !== null)
                {
                    rv = r.match(re);
                    if (rv !== null)
                    {
                        yearFound = true;
                    }

                    copyrightFound = true;
                }
            }
            a.Close();

            if (copyrightFound && !yearFound)
            {
                if (errormsg !== "")
                {
                    errormsg += "\n";
                }
                errormsg += f;
                found = false;
            }
        }
    }
}

if (found === false)
{
    errormsg = "the file(s):\n" + errormsg + "\nhave not the correct copyright year!";
    WScript.stderr.writeLine(errormsg);
}

WScript.Quit(!found);
