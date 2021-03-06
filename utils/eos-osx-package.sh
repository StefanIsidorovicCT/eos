#!/bin/bash
# give the desired versioon name like '0.3.49-beryl' and the path to the XRootD build directory as argument like '/Users/apeters/Software/xrootd-4.3.0/build'
VERSION=$1

create_dmg_with_icon() {
set -e
VOLNAME="$1"
DMG="$2"
SRC_DIR="$3"
ICON_FILE="$4"
CODESIGN_IDENTITY="$5"

TMP_DMG="$(mktemp -u -t XXXXXXX)"
trap 'RESULT=$?; rm -f "$TMP_DMG"; exit $RESULT' INT QUIT TERM EXIT
hdiutil create -srcfolder "$SRC_DIR" -volname "$VOLNAME" -fs HFS+ \
               -fsargs "-c c=64,a=16,e=16" -format UDRW "$TMP_DMG"
TMP_DMG="${TMP_DMG}.dmg" # because OSX appends .dmg
DEVICE="$(hdiutil attach -readwrite -noautoopen "$TMP_DMG" | awk 'NR==1{print$1}')"
VOLUME="$(mount | grep "$DEVICE" | sed 's/^[^ ]* on //;s/ ([^)]*)$//')"
# start of DMG changes
cp "$ICON_FILE" "$VOLUME/.VolumeIcon.icns"
SetFile -c icnC "$VOLUME/.VolumeIcon.icns"
SetFile -a C "$VOLUME"
# end of DMG changes
hdiutil detach "$DEVICE"
hdiutil convert "$TMP_DMG" -format UDZO -imagekey zlib-level=9 -o "$DMG"
if [ -n "$CODESIGN_IDENTITY" ]; then
  codesign -s "$CODESIGN_IDENTITY" -v "$DMG"
fi
}

rm -rf /tmp/eos.dst/
mkdir -p /tmp/eos.dst/
mkdir -p /tmp/eos.dst/usr/local/bin/

make install DESTDIR=/tmp/eos.dst/
cd ${2-${HOME}/Software/xrootd-4.3.0/build/}
make install DESTDIR=/tmp/eos.dst/
cd -

for name in `otool -L $3/usr/local/bin/eosd | grep -v rpath | grep /usr/local/ | awk '{print $1}' | grep -v ":" | grep -v libosxfuse`; do
echo $name
if [ -n "$name" ];  then
  dn=`dirname $name`;
  mkdir -p /tmp/eos.dst/$dn/
  cp -v $name /tmp/eos.dst/$name
fi
done

# exchange the eosx script with the eos binary
mv /tmp/eos.dst/usr/local/bin/eos /tmp/eos.dst/usr/local/bin/eos.exe
cp -v ../utils/eosx /tmp/eos.dst/usr/local/bin/eos
chmod ugo+rx /tmp/eos.dst/usr/local/bin/eos

pkgbuild --install-location / --version $VERSION --identifier com.eos.pkg.app --root /tmp/eos.dst EOS.pkg

rm -rf dmg
mkdir dmg
cp EOS.pkg dmg/
cp ../utils/README.osx dmg/README.txt

#cp ../var/eos/html/EOS-logo.jpg dmg/
unlink eos-osx-$VERSION.dmg >& /dev/null
create_dmg_with_icon eos-osx-$VERSION eos-osx-$VERSION.dmg dmg ../icons/EOS.icns
# create_dmg_with_icon Frobulator Frobulator.dmg path/to/frobulator/dir path/to/someicon.icns [ 'Andreas-Joachim Peters' ]

