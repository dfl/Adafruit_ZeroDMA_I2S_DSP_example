#include <Adafruit_ZeroI2S.h>
#include <Adafruit_ZeroDMA.h>
#include "utility/dma.h"
#include <advancedSerial.h>

#define DEBUG 1 // print audio vs play audio

/* create a buffer for both the left and right channel data */
#define BUFSIZE 10
int data[BUFSIZE];

Adafruit_ZeroDMA myDMA;
ZeroDMAstatus    stat; // DMA status codes returned by some functions

Adafruit_ZeroI2S i2s; // using default pins

#include "sine_wave.h"
#define SRATE 22050
ModOsc sine(SRATE);

/*the I2S module will be expecting data interleaved LRLR*/
void inline fillBuffer() {  
  int *ptr = (int*)data;
  int osc;
  for(int i=0; i<BUFSIZE/2; i++){
    osc = sine.process() >> 3;
    *ptr++ = osc;
    *ptr++ = osc;
  }
}

void printBuffer() {
  int *ptr = (int*)data;
  int osc;
  Serial.print("AUDIO_BUFFER = {");
  for(int i=0; i<BUFSIZE/2; i++){
    Serial.print("  L: ");
    Serial.print( *ptr++, HEX );
    Serial.print(" R: ");
    Serial.println( *ptr++, HEX );
  }
  Serial.println("};");
  
}

volatile long callbackCount;
void dma_callback(Adafruit_ZeroDMA *dma) {
  fillBuffer();
  callbackCount++;
  myDMA.startJob();  
}

void setup() {
  Serial.begin(9600);
  while(!Serial);                 // Wait for Serial monitor before continuing

  setup_DMA();
  i2s.begin(I2S_32_BIT, SRATE);
  i2s.enableTx();


//  SineTable::print();
//  delay(2000);

  sine.setFrequency( 220.0 );
//  sine.setMod( 1.0 );
  sine.setGain(1.0);
  if(!DEBUG) {
    fillBuffer();
    stat = myDMA.startJob();
  }
}

void loop() {
  if(DEBUG) {
    sine.process();
  } else {
    Serial.println("do other things here while your DMA runs in the background.");
    Serial.print("   DMA callback count: ");
    Serial.println(callbackCount);
  }
//  delay(10);
}


void setup_DMA() {
  Serial.println("I2S output via DMA");

  Serial.println("Configuring DMA trigger");
  myDMA.setTrigger(I2S_DMAC_ID_TX_0);
  myDMA.setAction(DMA_TRIGGER_ACTON_BEAT);

  Serial.print("Allocating DMA channel...");
  stat = myDMA.allocate();
  myDMA.printStatus(stat);

  Serial.println("Setting up transfer");
  DmacDescriptor *dmaDesc = myDMA.addDescriptor(
      (void*)data,                    // move data from here
#if defined(__SAMD51__)
      (void *)(&I2S->TXDATA.reg), // to here (M4)
#else
      (void *)(&I2S->DATA[0].reg), // to here (M0+)
#endif
    BUFSIZE,                      // this many...
      DMA_BEAT_SIZE_WORD,               // bytes/hword/words
      true,                             // increment source addr?
      false);
  dmaDesc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT; //https://forums.adafruit.com/viewtopic.php?f=8&t=146979#p767883
  myDMA.loop(false);

  Serial.println("Adding callback");
  myDMA.setCallback(dma_callback);
  Serial.println();  
}
