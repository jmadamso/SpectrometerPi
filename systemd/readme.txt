place the contents of this folder into etc/systemd/system to 
auto-start the python script.

remove the .service tag to keep it from running

stop the service with:
	sudo systemctl stop BTServerPython

[other systemctl commands: start,restart]