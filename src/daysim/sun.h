
/*  Copyright (c) 2002
 *  National Research Council Canada
 *  Fraunhofer Institute for Solar Energy Systems
 *  written by Christoph Reinhart
 */

#pragma once

int    jdate( int month, int day);     /* Julian date (days into year) */
double stadj( int jd);                 /* solar time adjustment from Julian date */
double sdec( int jd);                  /* solar declination angle from Julian date */
double solar_sunset(int month, int day);
double sazi( double sd,  double st);	 /* solar azimuth from solar declination and solar time */
double salt( double sd, double st);	   /* solar altitude from solar declination and solar time */
double f_salt( double sd, double alt); /* solar altitude from solar declination and solar time */

void solar_elev_azi_ecc(double latitude, double longitude, double time_zone, int jday, double time, int solar_time, double *solar_elevation, double *solar_azimuth, double *eccentricity_correction);

double diffuse_fraction(double irrad_glo, double solar_elevation, double eccentricity_correction);

void sunrise_sunset_localtime(double latitude, double longitude, double time_zone, int jday, double *sunrise_localtime, double *sunset_localtime);
