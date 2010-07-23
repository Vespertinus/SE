/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2009 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

/* Vespertinus ___Start___

   this file based on Ogre Engine source
   
   Vespertinus ___End____ */

#ifndef __Vector3_H__
#define __Vector3_H__ 1

//#include "OgrePrerequisites.h"
//#include "OgreMath.h"
//#include "OgreQuaternion.h"

namespace SE {

namespace MATH {

	/** \addtogroup Core
	*  @{
	*/
	/** \addtogroup Math
	*  @{
	*/
	/** Standard 3-dimensional vector.
        @remarks
            A direction in 3D space represented as distances along the 3
            orthogonal axes (x, y, z). Note that positions, directions and
            scaling factors can be represented by a vector, depending on how
            you interpret the values.
    */
struct Vector3 {

		float   x, y, z;

    
    Vector3();
    Vector3( const float fX, const float fY, const float fZ );
    explicit Vector3( const float & af_coordinate[3] );
    explicit Vector3( const int   & af_coordinate[3] );
    explicit Vector3( const float * const r );
    explicit Vector3( const float scaler );
		
    /** Exchange the contents of this vector with another. 
		*/
		void Swap(Vector3& other)

		float operator [] ( const size_t i ) const;
		float& operator [] ( const size_t i );
		
    /// Pointer accessor for direct copying
		float* Ptr();
		/// Pointer accessor for direct copying
		const float* Ptr() const;

    /** Assigns the value of the other vector.
      @param rkVector       The other vector
     */
    Vector3& operator = ( const Vector3& rkVector );
    Vector3& operator = ( const float fScaler );
    bool operator == ( const Vector3& rkVector ) const;
    bool operator != ( const Vector3& rkVector ) const;

    // arithmetic operations
    Vector3 operator + ( const Vector3& rkVector ) const;
    Vector3 operator - ( const Vector3& rkVector ) const;
    Vector3 operator * ( const float fScalar ) const;
    Vector3 operator * ( const Vector3& rhs) const;
    Vector3 operator / ( const float fScalar ) const;
    Vector3 operator / ( const Vector3& rhs) const;
    const Vector3& operator + () const;
    Vector3 operator - () const;

    // overloaded operators to help Vector3
    friend Vector3 operator * ( const float fScalar, const Vector3& rkVector );
    friend Vector3 operator / ( const float fScalar, const Vector3& rkVector );
    friend Vector3 operator + (const Vector3& lhs, const float rhs);
    friend Vector3 operator + (const float lhs, const Vector3& rhs);
    friend Vector3 operator - (const Vector3& lhs, const float rhs);
    friend Vector3 operator - (const float lhs, const Vector3& rhs);

    // arithmetic updates
    Vector3& operator += ( const Vector3& rkVector );
    Vector3& operator += ( const float fScalar );
    Vector3& operator -= ( const Vector3& rkVector );
    Vector3& operator -= ( const float fScalar );
    Vector3& operator *= ( const float fScalar );
    Vector3& operator *= ( const Vector3& rkVector );
    Vector3& operator /= ( const float fScalar );
    Vector3& operator /= ( const Vector3& rkVector );

    /** Returns the length (magnitude) of the vector.
      @warning
      This operation requires a square root and is expensive in
      terms of CPU operations. If you don't need to know the exact
      length (e.g. for just comparing lengths) use squaredLength()
      instead.
     */
    float length () const;

    /** Returns the square of the length(magnitude) of the vector.
      @remarks
      This  method is for efficiency - calculating the actual
      length of a vector requires a square root, which is expensive
      in terms of the operations required. This method returns the
      square of the length of the vector, i.e. the same as the
      length but before the square root is taken. Use this if you
      want to find the longest / shortest vector without incurring
      the square root.
     */
    float squaredLength () const;

    /** Returns the distance to another vector.
      @warning
      This operation requires a square root and is expensive in
      terms of CPU operations. If you don't need to know the exact
      distance (e.g. for just comparing distances) use squaredDistance()
      instead.
     */
    float distance(const Vector3& rhs) const;

    /** Returns the square of the distance to another vector.
      @remarks
      This method is for efficiency - calculating the actual
      distance to another vector requires a square root, which is
      expensive in terms of the operations required. This method
      returns the square of the distance to another vector, i.e.
      the same as the distance but before the square root is taken.
      Use this if you want to find the longest / shortest distance
      without incurring the square root.
     */
    float squaredDistance(const Vector3& rhs) const;

    /** Calculates the dot (scalar) product of this vector with another.
      @remarks
      The dot product can be used to calculate the angle between 2
      vectors. If both are unit vectors, the dot product is the
      cosine of the angle; otherwise the dot product must be
      divided by the product of the lengths of both vectors to get
      the cosine of the angle. This result can further be used to
      calculate the distance of a point from a plane.
      @param
      vec Vector with which to calculate the dot product (together
      with this one).
      @returns
      A float representing the dot product value.
     */
    float dotProduct(const Vector3& vec) const;

    /** Calculates the absolute dot (scalar) product of this vector with another.
      @remarks
      This function work similar dotProduct, except it use absolute value
      of each component of the vector to computing.
      @param
      vec Vector with which to calculate the absolute dot product (together
      with this one).
      @returns
      A float representing the absolute dot product value.
     */
    float absDotProduct(const Vector3& vec) const;

    /** Normalises the vector.
      @remarks
      This method normalises the vector such that it's
      length / magnitude is 1. The result is called a unit vector.
      @note
      This function will not crash for zero-sized vectors, but there
      will be no changes made to their components.
      @returns The previous length of the vector.
     */
    float normalise();

    /** Calculates the cross-product of 2 vectors, i.e. the vector that
      lies perpendicular to them both.
      @remarks
      The cross-product is normally used to calculate the normal
      vector of a plane, by calculating the cross-product of 2
      non-equivalent vectors which lie on the plane (e.g. 2 edges
      of a triangle).
      @param
      vec Vector which, together with this one, will be used to
      calculate the cross-product.
      @returns
      A vector which is the result of the cross-product. This
      vector will <b>NOT</b> be normalised, to maximise efficiency
      - call Vector3::normalise on the result if you wish this to
      be done. As for which side the resultant vector will be on, the
      returned vector will be on the side from which the arc from 'this'
      to rkVector is anticlockwise, e.g. UNIT_Y.crossProduct(UNIT_Z)
      = UNIT_X, whilst UNIT_Z.crossProduct(UNIT_Y) = -UNIT_X.
      This is because OGRE uses a right-handed coordinate system.
      @par
      For a clearer explanation, look a the left and the bottom edges
      of your monitor's screen. Assume that the first vector is the
      left edge and the second vector is the bottom edge, both of
      them starting from the lower-left corner of the screen. The
      resulting vector is going to be perpendicular to both of them
      and will go <i>inside</i> the screen, towards the cathode tube
      (assuming you're using a CRT monitor, of course).
     */
    Vector3 crossProduct( const Vector3& rkVector ) const;

    /** Returns a vector at a point half way between this and the passed
      in vector.
     */
    Vector3 midPoint( const Vector3& vec ) const;

    /** Returns true if the vector's scalar components are all greater
      that the ones of the vector it is compared against.
     */
    bool operator < ( const Vector3& rhs ) const;

    /** Returns true if the vector's scalar components are all smaller
      that the ones of the vector it is compared against.
     */
    bool operator > ( const Vector3& rhs ) const;

    /** Sets this vector's components to the minimum of its own and the
      ones of the passed in vector.
      @remarks
      'Minimum' in this case means the combination of the lowest
      value of x, y and z from both vectors. Lowest is taken just
      numerically, not magnitude, so -1 < 0.
     */
    void makeFloor( const Vector3& cmp );

    /** Sets this vector's components to the maximum of its own and the
      ones of the passed in vector.
      @remarks
      'Maximum' in this case means the combination of the highest
      value of x, y and z from both vectors. Highest is taken just
      numerically, not magnitude, so 1 > -3.
     */
    void makeCeil( const Vector3& cmp );

    /** Generates a vector perpendicular to this vector (eg an 'up' vector).
      @remarks
      This method will return a vector which is perpendicular to this
      vector. There are an infinite number of possibilities but this
      method will guarantee to generate one of them. If you need more
      control you should use the Quaternion class.
     */
    Vector3 perpendicular(void) const
    {
      static const float fSquareZero = (float)(1e-06 * 1e-06);

      Vector3 perp = this->crossProduct( Vector3::UNIT_X );

      // Check length
      if( perp.squaredLength() < fSquareZero )
      {
        /* This vector is the Y axis multiplied by a scalar, so we have
           to use another axis.
         */
        perp = this->crossProduct( Vector3::UNIT_Y );
      }
      perp.normalise();

      return perp;
    }
    /** Generates a new random vector which deviates from this vector by a
      given angle in a random direction.
      @remarks
      This method assumes that the random number generator has already
      been seeded appropriately.
      @param
      angle The angle at which to deviate
      @param
      up Any vector perpendicular to this one (which could generated
      by cross-product of this vector and any other non-colinear
      vector). If you choose not to provide this the function will
      derive one on it's own, however if you provide one yourself the
      function will be faster (this allows you to reuse up vectors if
      you call this method more than once)
      @returns
      A random vector which deviates from this vector by angle. This
      vector will not be normalised, normalise it if you wish
      afterwards.
     */
    Vector3 randomDeviant(
        const Radian& angle,
        const Vector3& up = Vector3::ZERO ) const
    {
      Vector3 newUp;

      if (up == Vector3::ZERO)
      {
        // Generate an up vector
        newUp = this->perpendicular();
      }
      else
      {
        newUp = up;
      }

      // Rotate up vector by random amount around this
      Quaternion q;
      q.FromAngleAxis( Radian(Math::UnitRandom() * Math::TWO_PI), *this );
      newUp = q * newUp;

      // Finally rotate this by given angle around randomised up
      q.FromAngleAxis( angle, newUp );
      return q * (*this);
    }

    /** Gets the angle between 2 vectors.
      @remarks
      Vectors do not have to be unit-length but must represent directions.
     */
    Radian angleBetween(const Vector3& dest)
    {
      float lenProduct = length() * dest.length();

      // Divide by zero check
      if(lenProduct < 1e-6f)
        lenProduct = 1e-6f;

      float f = dotProduct(dest) / lenProduct;

      f = Math::Clamp(f, (float)-1.0, (float)1.0);
      return Math::ACos(f);

    }
    /** Gets the shortest arc quaternion to rotate this vector to the destination
      vector.
      @remarks
      If you call this with a dest vector that is close to the inverse
      of this vector, we will rotate 180 degrees around the 'fallbackAxis'
      (if specified, or a generated axis if not) since in this case
      ANY axis of rotation is valid.
     */
    Quaternion getRotationTo(const Vector3& dest,
        const Vector3& fallbackAxis = Vector3::ZERO) const
    {
      // Based on Stan Melax's article in Game Programming Gems
      Quaternion q;
      // Copy, since cannot modify local
      Vector3 v0 = *this;
      Vector3 v1 = dest;
      v0.normalise();
      v1.normalise();

      float d = v0.dotProduct(v1);
      // If dot == 1, vectors are the same
      if (d >= 1.0f)
      {
        return Quaternion::IDENTITY;
      }
      if (d < (1e-6f - 1.0f))
      {
        if (fallbackAxis != Vector3::ZERO)
        {
          // rotate 180 degrees about the fallback axis
          q.FromAngleAxis(Radian(Math::PI), fallbackAxis);
        }
        else
        {
          // Generate an axis
          Vector3 axis = Vector3::UNIT_X.crossProduct(*this);
          if (axis.isZeroLength()) // pick another if colinear
            axis = Vector3::UNIT_Y.crossProduct(*this);
          axis.normalise();
          q.FromAngleAxis(Radian(Math::PI), axis);
        }
      }
      else
      {
        float s = Math::Sqrt( (1+d)*2 );
        float invs = 1 / s;

        Vector3 c = v0.crossProduct(v1);

        q.x = c.x * invs;
        q.y = c.y * invs;
        q.z = c.z * invs;
        q.w = s * 0.5f;
        q.normalise();
      }
      return q;
    }

    /** Returns true if this vector is zero length. */
    bool isZeroLength(void) const
    {
      float sqlen = (x * x) + (y * y) + (z * z);
      return (sqlen < (1e-06 * 1e-06));

    }

    /** As normalise, except that this vector is unaffected and the
      normalised vector is returned as a copy. */
    Vector3 normalisedCopy(void) const
    {
      Vector3 ret = *this;
      ret.normalise();
      return ret;
    }

    /** Calculates a reflection vector to the plane with the given normal .
      @remarks NB assumes 'this' is pointing AWAY FROM the plane, invert if it is not.
     */
    Vector3 reflect(const Vector3& normal) const
    {
      return Vector3( *this - ( 2 * this->dotProduct(normal) * normal ) );
    }

    /** Returns whether this vector is within a positional tolerance
      of another vector.
      @param rhs The vector to compare with
      @param tolerance The amount that each element of the vector may vary by
      and still be considered equal
     */
    bool positionEquals(const Vector3& rhs, float tolerance = 1e-03) const
    {
      return Math::floatEqual(x, rhs.x, tolerance) &&
        Math::floatEqual(y, rhs.y, tolerance) &&
        Math::floatEqual(z, rhs.z, tolerance);

    }

    /** Returns whether this vector is within a positional tolerance
      of another vector, also take scale of the vectors into account.
      @param rhs The vector to compare with
      @param tolerance The amount (related to the scale of vectors) that distance
      of the vector may vary by and still be considered close
     */
    bool positionCloses(const Vector3& rhs, float tolerance = 1e-03f) const
    {
      return squaredDistance(rhs) <=
        (squaredLength() + rhs.squaredLength()) * tolerance;
    }

    /** Returns whether this vector is within a directional tolerance
      of another vector.
      @param rhs The vector to compare with
      @param tolerance The maximum angle by which the vectors may vary and
      still be considered equal
      @note Both vectors should be normalised.
     */
    bool directionEquals(const Vector3& rhs,
        const Radian& tolerance) const
    {
      float dot = dotProduct(rhs);
      Radian angle = Math::ACos(dot);

      return Math::Abs(angle.valueRadians()) <= tolerance.valueRadians();

    }

    /// Check whether this vector contains valid values
    bool isNaN() const
    {
      return Math::isNaN(x) || Math::isNaN(y) || Math::isNaN(z);
    }

    // special points
    static const Vector3 ZERO;
    static const Vector3 UNIT_X;
    static const Vector3 UNIT_Y;
    static const Vector3 UNIT_Z;
    static const Vector3 NEGATIVE_UNIT_X;
    static const Vector3 NEGATIVE_UNIT_Y;
    static const Vector3 NEGATIVE_UNIT_Z;
    static const Vector3 UNIT_SCALE;

    /** Function for writing to a stream.
     */
    _OgreExport friend std::ostream& operator <<
      ( std::ostream& o, const Vector3& v )
      {
        o << "Vector3(" << v.x << ", " << v.y << ", " << v.z << ")";
        return o;
      }
};
/** @} */
/** @} */

} // namespace MATH
} // namespace SE

#endif
