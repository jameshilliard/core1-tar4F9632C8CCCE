import string, sys

if len(sys.argv)>1:
    input_file=open(sys.argv[1])
else:
    input_file=sys.stdin
data=input_file.read()
index=data.rfind('.')
if index==-1:
    print "No number use '.' to separate"
    sys.exit(1)
begin_i = index+1
index+=1
while data[index] in "0123456789":
    index+=1
value = "%d"%(string.atoi(data[begin_i:index])+1)
if len(sys.argv)>1:
    output_file=open(sys.argv[1],"w")
else:
    output_file=sys.stdout
output_file.write(data[:begin_i]+value+data[index:])

