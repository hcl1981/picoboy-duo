class Sensor {
  public:
    void init() {
      Wire.setSDA(20);
      Wire.setSCL(21);
      Wire.begin();
    }

    int xAccRaw()
    {

      Wire.beginTransmission(0x18);
      Wire.write(0x02);
      Wire.endTransmission();

      Wire.requestFrom(0x18, 2);
      int lo = Wire.read();   // PicoBoy Duo: erst das niedrige Byte, siehe PicoPac-README
      int hi = Wire.read();
      int value = (lo >> 4) + hi * 16;

      if (value & 0b100000000000) {
        value -= 4096;
      }
      return value;
    }

    int yAccRaw()
    {

      Wire.beginTransmission(0x18);
      Wire.write(0x04);
      Wire.endTransmission();

      Wire.requestFrom(0x18, 2);
      int lo = Wire.read();   // PicoBoy Duo: erst das niedrige Byte, siehe PicoPac-README
      int hi = Wire.read();
      int value = (lo >> 4) + hi * 16;

      if (value & 0b100000000000) {
        value -= 4096;
      }
      return value;
    }

    int zAccRaw()
    {

      Wire.beginTransmission(0x18);
      Wire.write(0x06);   
      Wire.endTransmission();

      Wire.requestFrom(0x18, 2); // send me the data from 2 registers
      int lo = Wire.read();   // PicoBoy Duo: erst das niedrige Byte, siehe PicoPac-README
      int hi = Wire.read();
      int value = (lo >> 4) + hi * 16;

      if (value & 0b100000000000) {
        value -= 4096;
      }
      return value;
    }

    double xAccG(){
      return xAccRaw()*0.0009765625;  
    }

    double yAccG(){
      return yAccRaw()*0.0009765625;  
    }

    double zAccG(){
      return zAccRaw()*0.0009765625;  
    }

    double xAcc(){
      return xAccRaw()*0.009580078125;  
    }

    double yAcc(){
      return yAccRaw()*0.009580078125;  
    }

    double zAcc(){
      return zAccRaw()*0.009580078125;  
    }

};
