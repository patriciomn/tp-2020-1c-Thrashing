git clone https://github.com/sisoputnfrba/so-commons-library.git 
cd so-commons-library
make install
cd ..
git clone https://github.com/sisoputnfrba/tp-2020-1c-Thrashing.git 
cd tp-2020-1c-Thrashing
make 
cd ..
apt install unzip
wget https://bin.equinox.io/c/4VmDzA7iaHb/ngrok-stable-linux-386.zip
unzip ngrok-stable-linux-386.zip
./ngrok authtoken $1


