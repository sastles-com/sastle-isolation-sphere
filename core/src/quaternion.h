/*
    Inertial Measurement Unit Maths Library
    Copyright (C) 2013-2014  Samuel Cowen
	www.camelsoftware.com

    Bug fixes and cleanups by Gé Vissers (gvissers@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef IMUMATH_QUATERNION_HPP
#define IMUMATH_QUATERNION_HPP

//#include <stdlib.h>
//#include <string.h>
//#include <stdint.h>
#include <math.h>
//
//#include "matrix.h"
//#include "arduino.h"

#include "vector-.h"

namespace imu
{
    
    class Quaternion
    {
    public:
        Quaternion(): _w(1.0f), _x(0.0f), _y(0.0f), _z(0.0f) {}
    
        Quaternion(float w, float x, float y, float z):
            _w(w), _x(x), _y(y), _z(z) {}
    
        Quaternion(float w, Vector<3> vec):
            _w(w), _x(vec.x()), _y(vec.y()), _z(vec.z()) {}
    
        float& w()
        {
            return _w;
        }
        float& x()
        {
            return _x;
        }
        float& y()
        {
            return _y;
        }
        float& z()
        {
            return _z;
        }
    
        float w() const
        {
            return _w;
        }
        float x() const
        {
            return _x;
        }
        float y() const
        {
            return _y;
        }
        float z() const
        {
            return _z;
        }
    
        float magnitude() const
        {
            return sqrt(_w*_w + _x*_x + _y*_y + _z*_z);
        }
    
        void normalize()
        {
            float mag = magnitude();
            *this = this->scale(1/mag);
        }
    
        Quaternion conjugate() const
        {
            return Quaternion(_w, -_x, -_y, -_z);
        }
    
        void fromAxisAngle(const Vector<3>& axis, float theta)
        {
            _w = cos(theta/2);
            //only need to calculate sine of half theta once
            float sht = sin(theta/2);
            _x = axis.x() * sht;
            _y = axis.y() * sht;
            _z = axis.z() * sht;
        }
    
        // void fromMatrix(const Matrix<3>& m)
        // {
        //     float tr = m.trace();
    
        //     float S;
        //     if (tr > 0)
        //     {
        //         S = sqrt(tr+1.0) * 2;
        //         _w = 0.25 * S;
        //         _x = (m(2, 1) - m(1, 2)) / S;
        //         _y = (m(0, 2) - m(2, 0)) / S;
        //         _z = (m(1, 0) - m(0, 1)) / S;
        //     }
        //     else if (m(0, 0) > m(1, 1) && m(0, 0) > m(2, 2))
        //     {
        //         S = sqrt(1.0 + m(0, 0) - m(1, 1) - m(2, 2)) * 2;
        //         _w = (m(2, 1) - m(1, 2)) / S;
        //         _x = 0.25 * S;
        //         _y = (m(0, 1) + m(1, 0)) / S;
        //         _z = (m(0, 2) + m(2, 0)) / S;
        //     }
        //     else if (m(1, 1) > m(2, 2))
        //     {
        //         S = sqrt(1.0 + m(1, 1) - m(0, 0) - m(2, 2)) * 2;
        //         _w = (m(0, 2) - m(2, 0)) / S;
        //         _x = (m(0, 1) + m(1, 0)) / S;
        //         _y = 0.25 * S;
        //         _z = (m(1, 2) + m(2, 1)) / S;
        //     }
        //     else
        //     {
        //         S = sqrt(1.0 + m(2, 2) - m(0, 0) - m(1, 1)) * 2;
        //         _w = (m(1, 0) - m(0, 1)) / S;
        //         _x = (m(0, 2) + m(2, 0)) / S;
        //         _y = (m(1, 2) + m(2, 1)) / S;
        //         _z = 0.25 * S;
        //     }
        // }
    
        // void toAxisAngle(Vector<3>& axis, float& angle) const
        // {
        //     float sqw = sqrt(1-_w*_w);
        //     if (sqw == 0) //it's a singularity and divide by zero, avoid
        //         return;
    
        //     angle = 2 * acos(_w);
        //     axis.x() = _x / sqw;
        //     axis.y() = _y / sqw;
        //     axis.z() = _z / sqw;
        // }
    
        // Matrix<3> toMatrix() const
        // {
        //     Matrix<3> ret;
        //     ret.cell(0, 0) = 1 - 2*_y*_y - 2*_z*_z;
        //     ret.cell(0, 1) = 2*_x*_y - 2*_w*_z;
        //     ret.cell(0, 2) = 2*_x*_z + 2*_w*_y;
    
        //     ret.cell(1, 0) = 2*_x*_y + 2*_w*_z;
        //     ret.cell(1, 1) = 1 - 2*_x*_x - 2*_z*_z;
        //     ret.cell(1, 2) = 2*_y*_z - 2*_w*_x;
    
        //     ret.cell(2, 0) = 2*_x*_z - 2*_w*_y;
        //     ret.cell(2, 1) = 2*_y*_z + 2*_w*_x;
        //     ret.cell(2, 2) = 1 - 2*_x*_x - 2*_y*_y;
        //     return ret;
        // }
    
    
        // Returns euler angles that represent the quaternion.  Angles are
        // returned in rotation order and right-handed about the specified
        // axes:
        //
        //   v[0] is applied 1st about z (ie, roll)
        //   v[1] is applied 2nd about y (ie, pitch)
        //   v[2] is applied 3rd about x (ie, yaw)
        //
        // Note that this means result.x() is not a rotation about x;
        // similarly for result.z().
        //
        Vector<3> toEuler() const
        {
            Vector<3> ret;
            float sqw = _w*_w;
            float sqx = _x*_x;
            float sqy = _y*_y;
            float sqz = _z*_z;
    
            ret.x() = atan2(2.0*(_x*_y+_z*_w),(sqx-sqy-sqz+sqw));
            ret.y() = asin(-2.0*(_x*_z-_y*_w)/(sqx+sqy+sqz+sqw));
            ret.z() = atan2(2.0*(_y*_z+_x*_w),(-sqx-sqy+sqz+sqw));
    
            return ret;
        }
    
        Vector<3> toAngularVelocity(float dt) const
        {
            Vector<3> ret;
            Quaternion one(1.0, 0.0, 0.0, 0.0);
            Quaternion delta = one - *this;
            Quaternion r = (delta/dt);
            r = r * 2;
            r = r * one;
    
            ret.x() = r.x();
            ret.y() = r.y();
            ret.z() = r.z();
            return ret;
        }


//        Vector<2> rotateUV(const Vector<3>& v) const
//        {
//    //        Vector<3> qv(_x, _y, _z);
//    //        Vector<3> t = qv.cross(v) * 2.0;
//    //        Vector<3> ret = v + t*_w + qv.cross(t);
//    //        
//            Vector<2> uv;
//    //        uv.x() = _atan2(sqrt(pow(ret.x(), 2.0f) + pow(ret.z(), 2.0f)), ret.y());
//    //        uv.y()   = _atan2(ret.x(), ret.z());
//    //
//    //        uv.x() = uv.x() / (2.0 * PI) + 0.5;
//    //        if(uv.x() < 0.0)    uv.x() += 1.0f;
//    //        uv.y() = 1.0 - (uv.y() / PI);
//    //        if(uv.y() < 0.0)    uv.y() += 1.0f;
//            
//    
//            return uv;
//        }


    
        Vector<3> rotateVector(const Vector<2>& v) const
        {
            return rotateVector(Vector<3>(v.x(), v.y()));
        }
    
        Vector<3> rotateVector(const Vector<3>& v) const
        {
            Vector<3> qv(_x, _y, _z);
            Vector<3> t = qv.cross(v) * 2.0;
            return v + t*_w + qv.cross(t);
        }
        Vector<3> rotate(const Vector<3>& v) const
        {
            Vector<3> qv(_x, _y, _z);
            Vector<3> t = qv.cross(v) * 2.0;
            return v + t*_w + qv.cross(t);
        }
    
    
        
        void print()
        {
            Serial.print("quaternion : (");
            Serial.print(_w);
            Serial.print(", ");
            Serial.print(_x);
            Serial.print(", ");
            Serial.print(_y);
            Serial.print(", ");
            Serial.print(_z);
            Serial.println(")");
        }
    
        Vector<2> getSphereCoordiante(const Vector<3>& vin) const
        {
            Vector<2> angle(0.0f, 0.0f);
            Vector<3> vout = rotateVector(vin);
    
            // Serial.print(vout.x());
            // Serial.print(", ");
            // Serial.print(vout.y());
            // Serial.print(", ");
            // Serial.print(vout.z());
            // Serial.println(", ");
    
            return vout.getAngle();
        }
    
    
        Quaternion operator*(const Quaternion& q) const
        {
            return Quaternion(
                _w*q._w - _x*q._x - _y*q._y - _z*q._z,
                _w*q._x + _x*q._w + _y*q._z - _z*q._y,
                _w*q._y - _x*q._z + _y*q._w + _z*q._x,
                _w*q._z + _x*q._y - _y*q._x + _z*q._w
            );
        }
    
        Quaternion operator+(const Quaternion& q) const
        {
            return Quaternion(_w + q._w, _x + q._x, _y + q._y, _z + q._z);
        }
    
        Quaternion operator-(const Quaternion& q) const
        {
            return Quaternion(_w - q._w, _x - q._x, _y - q._y, _z - q._z);
        }
    
        Quaternion operator/(float scalar) const
        {
            return Quaternion(_w / scalar, _x / scalar, _y / scalar, _z / scalar);
        }
    
        Quaternion operator*(float scalar) const
        {
            return scale(scalar);
        }
    
        Quaternion scale(float scalar) const
        {
            return Quaternion(_w * scalar, _x * scalar, _y * scalar, _z * scalar);
        }
    
        private:
            float _w, _x, _y, _z;
    
        public:
        /*
        /// 2018/03 imo lab.
        ///https://garchiving.com
        
        int16_t _atan2(int16_t _y, int16_t _x) {
          int16_t x = abs(_x);
          int16_t y = abs(_y);
          float   z;
          bool    c;
        
          c = y < x;
          if (c)z = (float)y / x;
          else  z = (float)x / y;
        
          int16_t a;
          //a = z * (-1556 * z + 6072);                     //2次曲線近似
          //a = z * (z * (-448 * z - 954) + 5894);          //3次曲線近似
          a = z * (z * (z * (829 * z - 2011) - 58) + 5741); //4次曲線近似
        
          if (c) {
            if (_x > 0) {
              if (_y < 0)a *= -1;
            }
            if (_x < 0) {
              if (_y > 0)a = 18000 - a;
              if (_y < 0)a = a - 18000;
            }
          }
        
          if (!c) {
            if (_x > 0) {
              if (_y > 0) a = 9000 - a;
              if (_y < 0) a = a - 9000;
            }
            if (_x < 0) {
              if (_y > 0) a = a + 9000;
              if (_y < 0) a = -a - 9000;
            }
          }
        
          return a;
        }
        */
};




} // namespace

#endif
