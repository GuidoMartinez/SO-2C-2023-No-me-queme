CWD=$PWD
COMMONS="so-commons-library"
cd ./cpu/
echo "\nCreando carpetas en: ${PWD}"
mkdir obj
cd ../kernel/
echo "\nCreando carpetas en: ${PWD}"
mkdir obj
cd ../memoria/
echo "\nCreando carpetas en: ${PWD}"
mkdir obj
cd ../filesystem/
echo "\nCreando carpetas en: ${PWD}"
mkdir obj
cd ../shared/
echo "\nCreando carpetas en: ${PWD}"
mkdir obj
cd $CD
echo "\nClonando commons..."
git clone "https://github.com/sisoputnfrba/${COMMONS}.git" $COMMONS
cd $COMMONS
echo "\nInstalando commons..."
sudo make install
cd $CD
echo "\nCreando carpetas para peristencia de FS"
mkdir fs
cd ./fs
mkdir fcbs
echo "\n\t\t INSTALACION FINALIZADA\n"