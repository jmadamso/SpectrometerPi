place the contents of this folder into etc/systemd/system to 
auto-start the python script. 
You also need to enable it (one-time only) at the command line:
	sudo systemctl enable BTServer
	reboot

remove the .service tag to keep it from running

stop the service with:
	sudo systemctl stop BTServer

[other systemctl commands: start,restart, enable, disable]