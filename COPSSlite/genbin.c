//===================================================== file = genbin.c =====
//=  Program to generate Binomial distributed random variables              =
//===========================================================================
//=  Notes: 1) Writes to a user specified output file                       =
//=         2) Generates user specified number of values                    =
//=-------------------------------------------------------------------------=
//= Example user input:                                                     =
//=                                                                         =
//=   ---------------------------------------- genbin.c ------              =
//=   -  Program to generate Binomial random variables       -              =
//=   --------------------------------------------------------              =
//=   Output file name ===================================> output.dat      =
//=   Random number seed =================================> 1               =
//=   Prob[success] value ================================> 0.1             =
//=   Number of trials ===================================> 20              =
//=   Number of values to generate =======================> 6               =
//=   --------------------------------------------------------              =
//=   -  Generating samples to file                          -              =
//=   --------------------------------------------------------              =
//=   --------------------------------------------------------              =
//=   -  Done!                                                              =
//=   --------------------------------------------------------              =
//=-------------------------------------------------------------------------=
//= Example output file ("output.dat" for above):                           =
//=                                                                         =
//=   6                                                                     =
//=   2                                                                     =
//=   2                                                                     =
//=   3                                                                     =
//=   1                                                                     =
//=   1                                                                     =
//=-------------------------------------------------------------------------=
//=  Build: bcc32 genbin.c                                                  =
//=-------------------------------------------------------------------------=
//=  Execute: genbin                                                        =
//=-------------------------------------------------------------------------=
//=  Author: Ken Christensen                                                =
//=          University of South Florida                                    =
//=          WWW: http://www.csee.usf.edu/~christen                         =
//=          Email: christen@csee.usf.edu                                   =
//=-------------------------------------------------------------------------=
//=  History: KJC (07/24/06) - Genesis (from genpois.c)                     =
//===========================================================================

//----- Include files -------------------------------------------------------
#include <stdio.h>              // Needed for printf()
#include <stdlib.h>             // Needed for exit() and ato*()

//----- Function prototypes -------------------------------------------------
int    binomial(double p, int n); // Returns a Binomial random variable
double bin_rand_val(int seed);        // Jain's RNG

//===== Main program ========================================================
void bmain(void)
{
  FILE     *fp;                 // File pointer to output file
  char     file_name[256];      // Output file name string
  char     temp_string[256];    // Temporary string variable
  int      n;                   // Number of trials
  double   p;                   // Probability of success
  int      bin_rv;              // Binomial random variable
  int      num_values;          // Number of values to generate
  int      i;                   // Loop counter

  // Output banner
  printf("---------------------------------------- genpois.c ----- \n");
  printf("-  Program to generate Poisson random variables        - \n");
  printf("-------------------------------------------------------- \n");

  // Prompt for output filename and then create/open the file
  printf("Output file name ===================================> ");
  scanf("%s", file_name);
  fp = fopen(file_name, "w");
  if (fp == NULL)
  {
    printf("ERROR in creating output file (%s) \n", file_name);
    exit(1);
  }

  // Prompt for random number seed and then use it
  printf("Random number seed (greater than 0) ================> ");
  scanf("%s", temp_string);
  bin_rand_val((int) atoi(temp_string));

  // Prompt for Prob[success]
  printf("Prob[success] value ================================> ");
  scanf("%s", temp_string);
  p = atof(temp_string);

  // Prompt for number of trials
  printf("Number of trials ====================================> ");
  scanf("%s", temp_string);
  n = atoi(temp_string);

  // Prompt for number of values to generate
  printf("Number of values to generate =======================> ");
  scanf("%s", temp_string);
  num_values = atoi(temp_string);

  //Output message and generate interarrival times
  printf("-------------------------------------------------------- \n");
  printf("-  Generating samples to file                          - \n");
  printf("-------------------------------------------------------- \n");

  // Generate and output binomial random variables
  for (i=0; i<num_values; i++)
  {
    bin_rv = binomial(p, n);
    fprintf(fp, "%d \n", bin_rv);
  }

  //Output message and close the output file
  printf("-------------------------------------------------------- \n");
  printf("-  Done! \n");
  printf("-------------------------------------------------------- \n");
  fclose(fp);
}

//===========================================================================
//=  Function to generate Binomial distributed random variables             =
//=    - Input:  p and n                                                    =
//=    - Output: Returns with Binomial distributed random variable          =
//===========================================================================
int binomial(double p, int n)
{
  int    bin_value;             // Computed Poisson value to be returned
  int    i;                     // Loop counter

  // Generate a binomial random variate
  bin_value = 0;
  for (i=0; i<n; i++)
    if (bin_rand_val(0) < p) bin_value++;

  return(bin_value);
}

//=========================================================================
//= Multiplicative LCG for generating uniform(0.0, 1.0) random numbers    =
//=   - x_n = 7^5*x_(n-1)mod(2^31 - 1)                                    =
//=   - With x seeded to 1 the 10000th x value should be 1043618065       =
//=   - From R. Jain, "The Art of Computer Systems Performance Analysis," =
//=     John Wiley & Sons, 1991. (Page 443, Figure 26.2)                  =
//=========================================================================
double bin_rand_val(int seed)
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

