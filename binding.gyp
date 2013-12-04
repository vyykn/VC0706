{
  "targets": [
    {
      "target_name": "VC0706",
      "sources": [ "vc0706.cpp" ],
      "dependencies": ["wiringPi", "wiringSerial"]
    },
    {
      "target_name": "wiringPi",
      "type": "static_library",
      "sources": [ "deps/wiringPi.c" ]
    },
    {
      "target_name": "wiringSerial",
      "type": "static_library",
      "sources": [ "deps/wiringSerial.c" ]
    }
  ]
}
