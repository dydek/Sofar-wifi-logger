### Sofar solar WIFI data logger

#### How does it work?


#### Setup

Before compilation you need to copy the config file 

```bash
cp src/Config.h.tpl src/Config.h
```

And add all your variables mentioned in that file (I guess wifi and PV monitor data are pretty obvious). The one tricky part might be the SOFAR_REQUEST code - for that I'm going to create a dedicated side because it's kind of tricy to explain how to get this variable.