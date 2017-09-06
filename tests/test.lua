#!/usr/bin/env lua5.1

local tmp = "/tmp"
local sep = string.match (package.config, "[^\n]+")
local upper = ".."

require("lfb")
print (Files._VERSION)

io.write(".")
io.flush()

function attrdir (path)
        for file in Files.dir(path) do
                if file ~= "." and file ~= ".." then
                        local f = path..sep..file
                        print ("\t=> "..f.." <=")
                        local attr = Files.attributes (f)
                        assert (type(attr) == "table")
                        if attr.mode == "directory" then
                                attrdir (f)
                        else
                                for name, value in pairs(attr) do
                                        print (name, value)
                                end
                        end
                end
        end
end

-- Checking changing directories
local current = assert (Files.currentdir())
local reldir = string.gsub (current, "^.*%"..sep.."([^"..sep.."])$", "%1")
assert (Files.chdir (upper), "could not change to upper directory")
assert (Files.chdir (reldir), "could not change back to current directory")
assert (Files.currentdir() == current, "error trying to change directories")
assert (Files.chdir ("this couldn't be an actual directory") == nil, "could change to a non-existent directory")

io.write(".")
io.flush()

-- Changing creating and removing directories
local tmpdir = current..sep.."Files_tmp_dir"
local tmpfile = tmpdir..sep.."tmp_file"
-- Test for existence of a previous Files_tmp_dir
-- that may have resulted from an interrupted test execution and remove it
if Files.chdir (tmpdir) then
    assert (Files.chdir (upper), "could not change to upper directory")
    assert (os.remove (tmpfile), "could not remove file from previous test")
    assert (Files.rmdir (tmpdir), "could not remove directory from previous test")
end

io.write(".")
io.flush()

-- tries to create a directory
assert (Files.mkdir (tmpdir), "could not make a new directory")
local attrib, errmsg = Files.attributes (tmpdir)
if not attrib then
        error ("could not get attributes of file `"..tmpdir.."':\n"..errmsg)
end
local f = io.open(tmpfile, "w")
f:close()

io.write(".")
io.flush()

-- Change access time
local testdate = os.time({ year = 2007, day = 10, month = 2, hour=0})
assert (Files.touch (tmpfile, testdate))
local new_att = assert (Files.attributes (tmpfile))
assert (new_att.access == testdate, "could not set access time")
assert (new_att.modification == testdate, "could not set modification time")

io.write(".")
io.flush()

-- Change access and modification time
local testdate1 = os.time({ year = 2007, day = 10, month = 2, hour=0})
local testdate2 = os.time({ year = 2007, day = 11, month = 2, hour=0})

assert (Files.touch (tmpfile, testdate2, testdate1))
local new_att = assert (Files.attributes (tmpfile))
assert (new_att.access == testdate2, "could not set access time")
assert (new_att.modification == testdate1, "could not set modification time")

io.write(".")
io.flush()

-- Checking link (does not work on Windows)
if Files.link (tmpfile, "_a_link_for_test_", true) then
  assert (Files.attributes"_a_link_for_test_".mode == "file")
  assert (Files.symlinkattributes"_a_link_for_test_".mode == "link")
  assert (Files.symlinkattributes"_a_link_for_test_".target == tmpfile)
  assert (Files.symlinkattributes("_a_link_for_test_", "target") == tmpfile)
  assert (Files.link (tmpfile, "_a_hard_link_for_test_"))
  assert (Files.attributes (tmpfile, "nlink") == 2)
  assert (os.remove"_a_link_for_test_")
  assert (os.remove"_a_hard_link_for_test_")
end

io.write(".")
io.flush()

-- Checking text/binary modes (only has an effect in Windows)
local f = io.open(tmpfile, "w")
local result, mode = Files.setmode(f, "binary")
assert(result) -- on non-Windows platforms, mode is always returned as "binary"
result, mode = Files.setmode(f, "text")
assert(result and mode == "binary")
f:close()
local ok, err = pcall(Files.setmode, f, "binary")
assert(not ok, "could setmode on closed file")
assert(err:find("closed file"), "bad error message for setmode on closed file")

io.write(".")
io.flush()

-- Restore access time to current value
assert (Files.touch (tmpfile, attrib.access, attrib.modification))
new_att = assert (Files.attributes (tmpfile))
assert (new_att.access == attrib.access)
assert (new_att.modification == attrib.modification)

io.write(".")
io.flush()

-- Check consistency of Files.attributes values
local attr = Files.attributes (tmpfile)
for key, value in pairs(attr) do
  assert (value == Files.attributes (tmpfile, key),
          "Files.attributes values not consistent")
end

-- Check that Files.attributes accepts a table as second argument
local attr2 = {}
Files.attributes(tmpfile, attr2)
for key, value in pairs(attr2) do
  assert (value == Files.attributes (tmpfile, key),
          "Files.attributes values with table argument not consistent")
end

-- Check that extra arguments are ignored
Files.attributes(tmpfile, attr2, nil)

-- Remove new file and directory
assert (os.remove (tmpfile), "could not remove new file")
assert (Files.rmdir (tmpdir), "could not remove new directory")
assert (Files.mkdir (tmpdir..sep.."Files_tmp_dir") == nil, "could create a directory inside a non-existent one")

io.write(".")
io.flush()

-- Trying to get attributes of a non-existent file
assert (Files.attributes ("this couldn't be an actual file") == nil, "could get attributes of a non-existent file")
assert (type(Files.attributes (upper)) == "table", "couldn't get attributes of upper directory")

io.write(".")
io.flush()

-- Stressing directory iterator
count = 0
for i = 1, 4000 do
        for file in Files.dir (tmp) do
                count = count + 1
        end
end

io.write(".")
io.flush()

-- Stressing directory iterator, explicit version
count = 0
for i = 1, 4000 do
  local iter, dir = Files.dir(tmp)
  local file = dir:next()
  while file do
    count = count + 1
    file = dir:next()
  end
  assert(not pcall(dir.next, dir))
end

io.write(".")
io.flush()

-- directory explicit close
local iter, dir = Files.dir(tmp)
dir:close()
assert(not pcall(dir.next, dir))
print"Ok!"
