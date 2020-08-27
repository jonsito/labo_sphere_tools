# labo_sphere_tools
Extra C programs and tools for labo_sphere webapp

Just a simple udp client / server program to be run on linux clients on lab to send server alive status, server info and logged users

At Server side program mantains a mmap'ed lookup table of clients status that in turn is checked by labo_sphere web app
client program is started as daemon on computer power on
Server tracks a timeout lookup table to mark as "off" computers when no "ping" signal received in last 5 minutes
