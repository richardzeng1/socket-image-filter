# socket-image-filter
A server that applies a series of filters on existing images or user uploaded images


![University of Toronto](https://upload.wikimedia.org/wikipedia/en/thumb/9/9a/UofT_Logo.svg/1280px-UofT_Logo.svg.png)


#### **Table of Contents**
1. Description
2. Setup
3. Usage


### **Description**
This is an assignment from the University of Toronto's Computer Science program. It is the fourth assignment from the course
CSC209. This assignment is based off image-filter. It uses sockets to apply the filters through a server on a web page. It allows
the 4 filters (excluding scale) to be applied to images currently in the server and have the new images be downloaded on the 
client. In addition, it allows the client to upload images to the server and these image can then have filters applied on it.

There is a lot of detail required in this assignment. See http://www.teach.cs.toronto.edu/~csc209h/fall/assignments/a4.html for additional detail.

Final Mark: 97.73%. <br />
Note: Some code was starter code written by the staff of the University of Toronto.

### **Setup**
1. Clone this repo
2. Change the port to an avaiable port on the server
3. $make
4. go on a web browser to http://localhost:<port>/main.html


### **Usage**

All interactions with the server are done through the web interface. 
 
