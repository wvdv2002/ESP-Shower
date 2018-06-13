
const char* showerState_str[] = {"IDLE","Start UV","Start Pump","showering","Top UV","TOP Pressure","PresHyst","Waiting"};

typedef enum showerstate{
  SHOWER_IDLE,
  SHOWER_STARTUV,
  SHOWER_STARTPUMP,
  SHOWER_SHOWERING,
  SHOWER_TOPUV,
  SHOWER_TOPPRESSURE,
  SHOWER_PRESSUREHYST,
  SHOWER_WAITING,
}showerState_t;
