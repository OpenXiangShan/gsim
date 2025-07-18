NAME=$1
DIR=build/$2
REF_DIR=emu/$3

rm -rf $REF_DIR
mkdir -p $REF_DIR
filelist=`find $DIR/model -name $NAME[0-9]*.cpp`
TMP_DIR=build/tmp
mkdir -p $TMP_DIR
for file in $filelist
do 
  name=`basename $file`
  echo $file
  sed "s/S$NAME/Diff$NAME/g" $file > $TMP_DIR/$name
  sed "s/gprintf/gprintf_ref/g" $TMP_DIR/$name > $TMP_DIR/${name}1
  sed "s/$NAME.h/top_ref.h/g" $TMP_DIR/${name}1 > $REF_DIR/ref_$name
done

sed "s/S$NAME/Diff$NAME/g" $DIR/model/$NAME.h > $TMP_DIR/top_ref.h
sed "s/gprintf/gprintf_ref/g" $TMP_DIR/top_ref.h > $TMP_DIR/top_ref1.h
sed "s/${NAME}_H/top_ref_H/g" $TMP_DIR/top_ref1.h > $REF_DIR/top_ref.h