# Weather widget in C for debian ubuntu

![Screenshot](widegt.png)

Do you remember Volkov commander? An assembly language clone of Norton Commander with only just 65kB in size?
I realized that python with my weather indicator is eating up my computing resources on slower machines and decided to write a minimalistic indicator showing only what I need. The compiled size is 45kB and resources consumed minimal. 
Courtesy of ChatGPT+Claude+Qwen and 3 hours of my spare time. 

Clone the repository: git clone https://github.com/kukulo2011/Weather-widget-in-C-for-debian-ubuntu/blob/MX-Linux-19-i386/
                      cd Weather-widget-in-C-for-debian-ubuntu


This is for MX Linux 19:

Installation prerequisities:

Icon prerequisities / courtesy of https://github.com/atareao/my-weather-indicator/ :

sudo add-apt-repository ppa:atareao/atareao
sudo apt update
sudo apt install my-weather-indicator


Compiling:

make clean-deps
make

Running:

./weather-widget&

Autostart:

Create a desktop file in:

~/.config/autostart/weather-widget.desktop

To move the widget use ALT + left mouse drag
