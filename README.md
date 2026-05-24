# Weather widget in C for debian ubuntu

https://github.com/kukulo2011/Weather-widget-in-C-for-debian-ubuntu/blob/main/widegt.png

Do you remember Volkov commander? An assembly language clone of Norton Commander with only just 65kB in size?
I realized that python with my weather indicator is eating up my computing resources on slower machines and decided to write a minimalistic indicator showing only what I need. The compiled size is 45kB and resources consumed minimal. 
Courtesy of ChatGPT+Claude+Qwen and 3 hours of my spare time. 

Clone the repository: git clone https://github.com/kukulo2011/Weather-widget-in-C-for-debian-ubuntu/
                      cd Weather-widget-in-C-for-debian-ubuntu

First, edit your longitude and lattitude for your accurate location on line 11-13 in the main.c.

Installation prerequisities:

sudo apt install -y \
  build-essential \
  pkg-config \
  libgtk-3-dev \
  libappindicator3-dev \
  libcurl4-openssl-dev \
  libjson-c-dev

Icon prerequisities / courtesy of https://github.com/atareao/my-weather-indicator/ :

sudo add-apt-repository ppa:atareao/atareao
sudo apt update
sudo apt install my-weather-indicator


Compiling:

gcc main.c -o weather $(pkg-config --cflags --libs gtk+-3.0 appindicator3-0.1 json-c libcurl)

Running:

./weather&

Autostart:

Put the following line in your autostart script on your OS:

/your_home_directory/Weather-widget-in-C-for-debian-ubuntu/weather


