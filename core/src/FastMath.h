#ifndef __FAST_MATH_H__
#define __FAST_MATH_H__

#include <cmath>

// LED座標変換(sphereToUV)用の高速近似計算関数群。
// 標準の sqrtf/atan2f とは数値結果が微妙に異なるため、
// 描画互換性を保つ目的で実装をそのまま維持している。

static inline float _sqrtinv(float a){

    float x, h, g;
    int e;

    // enhalf the exponent for a half digit initial accuracy
    frexp(a, &e);
    x = ldexp(1.0, -e >> 1);
//    Serial.printf("    a=%6.4f  e = %6.4f   x = %6.4f \n", a, e, x);

    // 1/sqrt(a) 4th order convergence
    g = 1.0;
    while(fabs(h=1.0-a*x*x) < fabs(g)){
        x += x * (h * (8.0 + h * (6.0 + 5.0 * h)) / 16.0);
        g = h;
//        Serial.printf("     G=%6.4f  x = %6.4f \n", g, x);
    }
    return(x);
}

static inline float _sqrt(float a){
    float ret;

    if(a < 0){
        return 0.0;
    }
    ret = a * _sqrtinv(a);
//    Serial.println(ret);

    return (ret);
}


static inline float _atan2(float _y, float _x){
    float x = abs(_x);
    float y = abs(_y);
    float z;
    bool c;


    if(y < x){
      z = y/x;
      c = true;
    }else{
      z = x/y;
      c = false;
    }
    float a = 8.0928*z*z*z*z-19.657*z*z*z-0.9258*z*z+57.511*z-0.0083;
    if(_x == 0.0){
        if(_y > 0.0)    a = 90.0;
        else            a = -90.0;
    }
    if(c){    // a<1
        if(_x > 0.0){
            if(_y < 0.0)  a *= -1.0;
        }
        if(_x < 0.0){
            if(_y > 0.0)  a = 180.0 - a;
            if(_y < 0.0)  a = a - 180.0;
        }
    }
    if(!c){   // a>1
        if(_x > 0.0){
            if(_y > 0.0)  a = 90.0 - a;
            if(_y < 0.0)  a = a - 90.0;
        }
        if(_x < 0.0){
            if(_y > 0.0)  a = a + 90.0;
            if(_y < 0.0)  a = -a - 90.0;
        }
    }
    // delay(1);
    // float rad = atan2(y, x);
    return a/180.0;
}

#endif /* __FAST_MATH_H__ */
