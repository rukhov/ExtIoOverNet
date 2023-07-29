# ExtIoOverNet
This software is intended to transmit IQ data from the different Software Defined Radios (SDR) to the network and receive it on another end for further process the data. As example I have a great SDR receiver SDRPlay RSP1A. It is placed in the roof flor of my house near to the antennas. I want to connect it to the network and be able to listen this radio remotely at my cabinet.
ExtIoOverNet software consists of two components. First is the ExtIO_OverNetServer.exe - this is a server that run on the machive to which the SDR is connected to. Second is a ExtIO_OverNetClient.dll - the client ExtIO API dll that is installed on the remote machine where is installed your favorite SDR software like HDSDR.
The important pecondition is that the SDR drivers and the SDR software both should support the ExtIO API.
## Build
You need cmake to build the project.
## Install and Run
* Copy the ExtIO_OverNetServer.exe server binary to the machine which the SDR hardware is connected to. Run the serve with command line parameters describel below.<br>
<b>--extio_path=<Path to the ExtIO_XXX.dll></b>  - ExtIO API dynamic linking library to be propagated over the network. This is mandatory parameter.<br>
<b>--listening_port=2056</b>  - The port number to be listened for client connections, default is 2056.<br>
<b>--log_level=0</b>  - Integer value of logging level: trace=0; debug=1; info=2; warning=3; error=4; fatal=5, default is 4.<br>
<b>extio_path</b> is mandatory parameter.
* Copy the ExtIO_OverNetClient.dll client ExtIO API module to the machine where is yours favorite SDR software is installed and where you are willing to play with a spectrum and to liten the radios. Create the config <b>ExtIO_OverNetClient.cfg</b> near the ExtIO_OverNetClient.dll. Add thwo mandatory parameters to the ExtIO_OverNetClient.cfg:<br>
<b>server_addr=127.0.0.1</b>  - ExtIoOverNet server address, default is localhost. This is a network address of machine where id yours SDR hardware is connected to. This is mandatory parameter.<br>
<b>server_port=2056</b>  - ExtIoOverNet server port, default is 2056. This is a port number which you configured by servers parameter <b>listening_port</b>. This is mandatory parameter.<br>
<b>log_level=0</b>  - Integer value of logging level: trace=0; debug=1; info=2; warning=3; error=4; fatal=5, default is 4. This is optionsl parameter.<br>
Run your favorite SDR software. Configure ExtIO_OverNetClient.dll as IQ data source im your favorite SDR software.<br>
That is it. It should work!)
