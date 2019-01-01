# Vuece libjingle

A customized and extended **libjingle** library that supports real-time music streaming and playing!


**vuece-libjingle** is a customized and extended version of the original [Google's libjingle library](https://developers.google.com/talk/libjingle/developer_guide), it is the core library that is driving
a real-time music steaming protocol developed for our Android application **Vuece Music Player** and the 
music file hosting application [Vuece Hub](http://www.vuece.com/). 


## Motivation
The motivation behind **vuece-libjingle** is that we want to find an innovative way of using original libjingle library, one 
of the most powerful aspects of libjingle is the [connection and session management](https://developers.google.com/talk/libjingle/make_receive_connections), based on that we can possibly build various applications like:
* Voice Call
* Video Call
* File Sharing

Other possible application scenarios are mentioned [here](https://developers.google.com/talk/libjingle/scenarios).

We decided to combine the existing functionalities provided by the original libjingle and extend them to a new model in which the 'caller' can initiate a session which is similar to existing phone call session but once the session is established the actual data is unidirectional streamable music data from the remote peer, thus enabling real-time music streaming to the caller, based on this model a libjingle-powered real-time music stream player will be made possible.
 
The output of this project include following three modules:
* **vuece-libjingle library** - The core library that drives the real-time streaming protocol
* **Vuece Music Controller** - an Android application that uses vuece-libjingle as the streaming engine and interacts with user like a normal music player, user guide and screenshots can be found [here](http://www.vuece.com/hub.html)
* **Vuecd Hub** - A windows application which hosts and streams music files using vuece-libjingle, detailed user guide and screenshots can be found [here](http://www.vuece.com/music.html)

The website hosting the applications is also a GitHub open-source project - [vuece.github.io](https://github.com/vuece/vuece.github.io)


## Getting Started

### Prerequisites
* Please get familiar with the official Google libjingle
* **vuece-libjingle** needs to be compiled in a Linux environment using [Android NDK](https://developer.android.com/ndk/) command line build tool. Since my host environment is Windows, I used **Virtualbox** with **Ubuntu** to develop and build the native library.


### Source Code and Vuece Stream Engine
* The [Android.mk file](vuece-libjingle/libjingle/Android.mk) contains all compilation configurations and listing of source files that will be compiled into the library
* Most of the customized source files use 'Vuece' has the file name prefix
* You might be interested in the following customized components under *talk/session/fileshare/* directory
    * **Vuece Stream Engine** - This module chains 3 components to form a channel that the real-time data will flow through 
    * **Vuece Media Data Bumper** - continuously reads encoded audio data frames and 'bumps' them to it's adjacent module for decoding, frame by frame
    * **Vuece AAC Decoder** - continuously reads and decodes frames received from the data bumper, the decoded frames flows into its adjacent module for audio frame writing
    * **Vuece Audio Writer** - the end of the streaming chain, it writes the audio data to the Audio Player through JNI callback


## Authors
Jingjing Sun - please drop a comment or contact me via [Linkedin](https://www.linkedin.com/in/jjsun001) if you need any further information.

