// #ifndef CORRELATION_h
// #define CORRELATION_h
// #ifdef ARDUINO
//     #if ARDUINO >= 100
//         #include "Arduino.h"
//         #else
//         #include "WProgram.h" /* This is where the standard Arduino code lies */
//     #endif
// #else
//     #include <stdlib.h>
//     #include <stdio.h>
//         #ifdef __AVR__
//         #include <avr/io.h>
//         #include <avr/pgmspace.h>
//         #endif
//     #include "defs.h"
//     #include "types.h"
//     #include <math.h>
// #endif


class Correlation {
public:
    Correlation(int32_t *firstSignal, int32_t *secondSignal, int numberOfSamples) {
        // constructor
        this->firstSignal = firstSignal;
        this->secondSignal = secondSignal;
        this->numberOfSamples = numberOfSamples;
    }


    double calculateCorrelationIndex(int index) {

        double result = 0;

        for (int i = index; i < index + this->numberOfSamples; i++) {
            result += (this->firstSignal[i] * this->secondSignal[i]) / 10;
        }

        return (result / numberOfSamples);
    }

private:
    int32_t *firstSignal;
    int32_t *secondSignal;
    int numberOfSamples;
}

// #endif