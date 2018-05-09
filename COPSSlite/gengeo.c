//===================================================== file = gengeo.c =====
//=  Program to generate geometrically distributed random variables         =
//=   - f(n) = p*(1-p)^(n-1) for n = 1, 2, 3, ...                           =
//===========================================================================
//=  Notes: 1) Writes to a user specified output file                       =
//=         2) Generates user specified number of values                    =
//=-------------------------------------------------------------------------=
//= Example user input:                                                     =
//=                                                                         =
//=   ----------------------------------------- gengeo.c -----              =
//=   -  Program to generate geometric random variables      -              =
//=   --------------------------------------------------------              =
//=   Output file name ===================================> x.txt           =
//=   Random number seed (greater than 0) ================> 1               =
//=   Probability of success (0 < p < 1) =================> 0.25            =
//=   Number of values to generate =======================> 5               =
//=   --------------------------------------------------------              =
//=   -  Generating samples to file                          -              =
//=   --------------------------------------------------------              =
//=   --------------------------------------------------------              =
//=   -  Done!                                                              =
//=   --------------------------------------------------------              =
//=-------------------------------------------------------------------------=
//= Example output file ("output.dat" for above):                           =
//=                                                                         =
//=  41                                                                     =
//=   8                                                                     =
//=   1                                                                     =
//=   3                                                                     =
//=   3                                                                     =
//=-------------------------------------------------------------------------=
//=  Build: bcc32 gengeo.c                                                  =
//=-------------------------------------------------------------------------=
//=  Execute: gengeo                                                        =
//=-------------------------------------------------------------------------=
//=  Author: Ken Christensen                                                =
//=          University of South Florida                                    =
//=          WWW: http://www.csee.usf.edu/~christen                         =
//=          Email: christen@csee.usf.edu                                   =
//=-------------------------------------------------------------------------=
//=  History: KJC (05/19/09) - Genesis (from genexp.c)                      =
//=           KJC (10/28/12) - Updated for support n = 1, 2, ...            =
//===========================================================================
//----- Include files -------------------------------------------------------
#include <stdio.h>            // Needed for printf()
#include <stdlib.h>           // Needed for exit() and ato*()
#include <math.h>             // Needed for log()

//----- Function prototypes -------------------------------------------------
int geo(double p);            // Returns a geometric random variable
double geo_rand_val(int seed);    // Jain's RNG

//===== Main program ========================================================
void gmain(void)
{
  char   in_string[256];      // Input string
  FILE   *fp;                 // File pointer to output file
  double p;                   // Probability of success for geometric
  int    geo_rv;              // Geometric random variable
  int    num_values;          // Number of values
  int    i;                   // Loop counter

  // Output banner
  printf("----------------------------------------- gengeo.c ----- \n");
  printf("-  Program to generate geometric random variables      - \n");
  printf("-------------------------------------------------------- \n");

  // Prompt for output filename and then create/open the file
  printf("Output file name ===================================> ");
  scanf("%s", in_string);
  fp = fopen(in_string, "w");
  if (fp == NULL)
  {
    printf("ERROR in creating output file (%s) \n", in_string);
    exit(1);
  }

  // Prompt for random number seed and then use it
  printf("Random number seed (greater than 0) ================> ");
  scanf("%s", in_string);
  geo_rand_val((int) atoi(in_string));

  // Prompt for probability of success
  printf("Probability of success (0 < p < 1) =================> ");
  scanf("%s", in_string);
  p = atof(in_string);

  // Prompt for number of values to generate
  printf("Number of values to generate =======================> ");
  scanf("%s", in_string);
  num_values = atoi(in_string);

  // Output message and generate interarrival times
  printf("-------------------------------------------------------- \n");
  printf("-  Generating samples to file                          - \n");
  printf("-------------------------------------------------------- \n");

  // Generate and output geometric random variables
  for (i=0; i<num_values; i++)
  {
    geo_rv = geo(p);
    fprintf(fp, "%d \n", geo_rv);
  }

  // Output message and close the output file
  printf("-------------------------------------------------------- \n");
  printf("-  Done! \n");
  printf("-------------------------------------------------------- \n");
  fclose(fp);
}

//===========================================================================
//=  Function to generate geometrically distributed random variables        =
//=    - Input:  Probability of success p                                   =
//=    - Output: Returns with geometrically distributed random variable     =
//===========================================================================
int geo(double p)
{
  double z;                     // Uniform random number (0 < z < 1)
  double geo_value;             // Computed geometric value to be returned

  // Pull a uniform random number (0 < z < 1)
  do
  {
    z = geo_rand_val(0);
  }
  while ((z == 0) || (z == 1));

  // Compute geometric random variable using inversion method
  geo_value = (int) (log(z) / log(1.0 - p)) + 1;

  return(geo_value);
}

//=========================================================================
//= Multiplicative LCG for generating uniform(0.0, 1.0) random numbers    =
//=   - x_n = 7^5*x_(n-1)mod(2^31 - 1)                                    =
//=   - With x seeded to 1 the 10000th x value should be 1043618065       =
//=   - From R. Jain, "The Art of Computer Systems Performance Analysis," =
//=     John Wiley & Sons, 1991. (Page 443, Figure 26.2)                  =
//=========================================================================
double geo_rand_val(int seed)
{
  const long  a =      16807;  // Multiplier
  const long  m = 2147483647;  // Modulus
  const long  q =     127773;  // m div a
  const long  r =       2836;  // m mod a
  static long x;               // Random int value
  long        x_div_q;         // x divided by q
  long        x_mod_q;         // x modulo q
  long        x_new;           // New x value

  // Set the seed if argument is non-zero and then return zero
  if (seed > 0)
  {
    x = seed;
    return(0.0);
  }

  // RNG using integer arithmetic
  x_div_q = x / q;
  x_mod_q = x % q;
  x_new = (a * x_mod_q) - (r * x_div_q);
  if (x_new > 0)
    x = x_new;
  else
    x = x_new + m;

  // Return a random value between 0.0 and 1.0
  return((double) x / m);
}

