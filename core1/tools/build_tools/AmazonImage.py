import getopt, os, string, sys


import tarfile, md5
import cStringIO

SoftwareVersion = "1.0.1"

AmazonImageDefaultPath = ["loader","bootconf","kernel","rootfs","firmware","userapp"]
HeadSize = 64

"""         Header Format
head ID :               4 bytes
upgrade image length:   4 bytes tar image part file
header MD5:             8 bytes md5sum (head ID+Length+'\x00'*8+image part MD5+
                                '\x00'*32+secret) then put below bytes
                                MD5[0]^MD5[8],MD5[1]^MD5[9]....to MD5[7]^MD5[15]
TarImagePart MD5:      16 bytes
padding         :      32 bytes ('\x00'*32 , reserved for future)

            Burn Images Part -- tar file format
tar image       :      N  bytes, must include more than one burning images
these images are put on below directories and will copy to /dev/mtd partitions:
loader/      bootconf/    kernel/      rootfs/       firmware/     userapp/
/dev/mtd/0   /dev/mtd/1   /dev/mtd/2   /dev/mtd/3    /dev/mtd/4    /dev/mtd/5
"""

def ordN(stream,n):
    return reduce(lambda x,y:long(x<<8)|long(y),map(ord,stream[:n]))

def hexN(stream,n):
    return "0x"+"%02x"*n%tuple(map(ord,stream[:n]))

def int32str(integer):
    return chr((integer>>24)&0xff)+chr((integer>>16)&0xff)+chr((integer>>8)&0xff)+chr(integer&0xff)

def usage_message():
    print "usage: python AmazonImage.py [create options] [-t output] [-s secret] imagefile"
    print "Version:%s"%SoftwareVersion
    print "No assigning [create options] will display imagefile information\n"
    print "-t: transfer to output which is TAR file format"
    print "-s: secret word (string length 0~16), default is ''\n"
    print "[create options]:  <sub-image options> <-v vendor_header>"
    print "-v: vendor header (string length must be 4), default is 'AMZN'"
    print "<sub-image options>: must assign one at least"
    print "-b: u-boot"
    print "-k: kernel"
    print "-f: firmware"
    print "-l: u-boot parameters"
    print "-r: root file system"
    print "-u: user configuration/rc.conf.gz"
    print "examples:"
    print "display image information, secret word is '12345678'"
    print "\tpython AmazonImage.py -s '12345678' myimage.bin"
    print "create image only include root file system image"
    print "\tpython AmazonImage.py -v 0x12345678 -r rootfs.img myimage"
    print "create image include u-boot, kernel and root file system image"
    print "\tpython AmazonImage.py -v VNID -b boot -k kernel -r rootfs myimage"

def headPart4CheckMD5(headID,length,imageMD5,secret):
    h = headID+int32str(length)+'\x00'*8+imageMD5+'\x00'*32+secret
    m = md5.md5(h).digest()
    result = ''
    for i in xrange(8):
        result+=chr(ord(m[i])^ord(m[i+8]))
    return result

def DisplayAmazonImage(filename,secret):
    data = open(filename,"rb").read()
    print "File Name:\t\t%s"%(filename)
    print "Length:\t\t\t%d (0x%x)"%(len(data),len(data))
    print "Vendor Header:\t\t%s ('%s')"%(hexN(data,4),data[:4])
    print "Burn Image Length:\t%d (0x%x)"%(ordN(data[4:],4),ordN(data[4:],4))
    if ordN(data[4:],4) != len(data)-HeadSize:
        print "!!Burn Image Length Incorrect!!"
    print "Header CheckSum:\t%s"%(hexN(data[8:],8))
    print "Burn Image MD5:\t\t%s"%(hexN(data[16:],16))
    print "Secret word:\t\t"+secret
    tarfileMD5 = md5.md5(data[HeadSize:]).digest()
    hc = headPart4CheckMD5(data[:4],ordN(data[4:],4),tarfileMD5,secret)
    if hc != data[8:16]:
        print "!!Head Checksum Incorrect!!"
    tar = tarfile.open(fileobj=cStringIO.StringIO(data[HeadSize:]), mode="r:")
    for tarinfo in tar:
        if tarinfo.isfile():
            print "Image Name: %-50s size: %d"%(tarinfo.name+',',tarinfo.size)
    tar.close()

def GetImages(filename, imgDict):
    tar =tarfile.open(mode="r:",fileobj=cStringIO.StringIO(open(filename,"rb").read()[HeadSize:]))
    for tarinfo in tar:
        if tarinfo.isfile():
            data = tar.extractfile(tarinfo).read()
            path, fname =os.path.split(tarinfo.name)
            if path in AmazonImageDefaultPath:
                imgDict[path] = [fname,data]
    tar.close()

def AmazonImage(fname='Amazon.img',headID='AMZN',secret='',setting={}):
    if len(setting) == 0:
        return
    temp = cStringIO.StringIO()
    tar = tarfile.open(mode="w:",fileobj=temp)
    for key in AmazonImageDefaultPath:
        if setting.has_key(key) and len(setting[key]):
            if not os.path.exists(setting[key]):
                print "%s not found" % (setting[key])
                tar.close()
                return
            path, name = os.path.split(setting[key])
            tarinfo = tar.gettarinfo(setting[key], key+"/"+name)
            tarinfo.uid = 0
            tarinfo.gid = 0
            if type(setting[key]) == str:
                tar.addfile(tarinfo, file(setting[key],"rb"))
            else:
                tar.addfile(tarinfo, cStringIO.StringIO(setting[key]))
    tar.close()
    length = temp.tell()
    tarfileMD5 = md5.md5(temp.getvalue()).digest()
    headcs = headPart4CheckMD5(headID[:4],length,tarfileMD5,secret)
    outfile = open(fname,'wb')
    outfile.write(headID[:4])
    outfile.write(int32str(length))
    outfile.write(headcs)
    outfile.write(tarfileMD5)
    outfile.write('\x00'*32)
    outfile.write(temp.getvalue())
    outfile.close()
    temp.close()

def RunAmazonImage():
    try:
        opts, args = getopt.getopt(sys.argv[1:], 't:s:b:k:l:u:r:v:f:')
    except getopt.error, msg:
        usage_message()
        sys.exit(2)
    secret = ''
    ftar = ''
    vendorId = 'AMZN'
    images = {}
    if len(args) != 1:
        usage_message()
        sys.exit(1)

    for o, a in opts:
        if o == '-b': images['loader'] = a
        if o == '-l': images['bootconf'] = a
        if o == '-k': images['kernel'] = a
        if o == '-r': images['rootfs'] = a
        if o == '-f': images['firmware'] = a
        if o == '-u': images['userapp'] = a
        if o == '-s': secret = a
        if o == '-t': ftar = a
        if o == '-v':
            if a[:2] == '0x':
                vendorId = int32str(string.atoi(a,16))
            else:
                vendorId = a

    if len(images) == 0:
        if len(ftar):
            print "Transfer image to tar file:" + ftar
            open(ftar,"wb").write(open(args[0],"rb").read()[HeadSize:])
        else:
            DisplayAmazonImage(args[0],secret)
    else:
        if len(ftar):
            print "Can't assign [-t] and [create options] in the same time"
        else:
            AmazonImage(args[0],vendorId,secret,images)


if __name__ == '__main__':
    RunAmazonImage()
    #a={}
    #GetImage("test.img",a)
