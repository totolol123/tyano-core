SIGNALS
=======

The server can be controlled using the following signals:

- `CHLD` refreshes the map.
- `CONT` reloads all data.
- `HUP`  saves all data.
- `QUIT` shuts down the server after saving it.
- `TERM` shuts down the server without saving it.
- `USR1` closes the server.
- `USR2` opens the server.

You can send a signal to the server using the commands `kill` or `killall`:

- `kill -SIG<signal> <process id>`  
  e.g. `kill -SIGTRAP 1234` to clean the map if the server is running with process ID `1234`.
  
- `killall -SIG<signal> <process name>`  
  e.g. `killall -SIGTRAP server` to clean the map if the server binary has the file name `server`.
