
#include "header.h"


static const int32_t num_class[] = {  1, };

int32_t get_num_target(void) {
  return N_TARGET;
}
void get_num_class(int32_t* out) {
  for (int i = 0; i < N_TARGET; ++i) {
    out[i] = num_class[i];
  }
}
int32_t get_num_feature(void) {
  return 5;
}
const char* get_threshold_type(void) {
  return "float64";
}
const char* get_leaf_output_type(void) {
  return "float64";
}

void predict(union Entry* data, int pred_margin, double* result) {
  unsigned int tmp;
  if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)42402.50000000000728) ) ) {
    if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)20800.00000000000364) ) ) {
      result[0] += 0.7027437105831329;
    } else {
      if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)38.50000000000000711) ) ) {
        if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)69515.00000000001455) ) ) {
          if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)30881.50000000000364) ) ) {
            if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)384.5000000000000568) ) ) {
              result[0] += 0.720144408125269;
            } else {
              result[0] += 0.7384920608755728;
            }
          } else {
            if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)345.5000000000000568) ) ) {
              result[0] += 0.7317712776121723;
            } else {
              if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)58697.50000000000728) ) ) {
                result[0] += 0.7524227953326018;
              } else {
                result[0] += 0.7373607428087;
              }
            }
          }
        } else {
          if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)19.50000000000000355) ) ) {
            result[0] += 0.7295167621168038;
          } else {
            result[0] += 0.7127032365973215;
          }
        }
      } else {
        if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)56744.00000000000728) ) ) {
          if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)32114.00000000000364) ) ) {
            if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)31842.50000000000364) ) ) {
              result[0] += 0.7425473590653374;
            } else {
              result[0] += 0.7061673121569104;
            }
          } else {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)55.50000000000000711) ) ) {
              result[0] += 0.7362150663779969;
            } else {
              result[0] += 0.7196864552892368;
            }
          }
        } else {
          if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)36752.50000000000728) ) ) {
            result[0] += 0.6859860647477154;
          } else {
            result[0] += 0.7057768437512371;
          }
        }
      }
    }
  } else {
    if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)190937.0000000000291) ) ) {
      if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)35.50000000000000711) ) ) {
        if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)56758.00000000000728) ) ) {
          if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)104746.5000000000146) ) ) {
            if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)19.50000000000000355) ) ) {
              result[0] += 0.7553898747378639;
            } else {
              if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)323.5000000000000568) ) ) {
                result[0] += 0.740985405983419;
              } else {
                result[0] += 0.7491238404504053;
              }
            }
          } else {
            result[0] += 0.7249398202719711;
          }
        } else {
          if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)22.50000000000000355) ) ) {
            result[0] += 0.756323702191456;
          } else {
            if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)98510.50000000001455) ) ) {
              result[0] += 0.7539167813179702;
            } else {
              if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)64276.50000000000728) ) ) {
                result[0] += 0.7311424870262487;
              } else {
                result[0] += 0.7497265424226789;
              }
            }
          }
        }
      } else {
        if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)53164.00000000000728) ) ) {
          if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)63193.50000000000728) ) ) {
            if ( LIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)50088.50000000000728) ) ) {
              result[0] += 0.7377663463419942;
            } else {
              result[0] += 0.7636927375329904;
            }
          } else {
            result[0] += 0.7221942451075418;
          }
        } else {
          if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)74078.50000000001455) ) ) {
            result[0] += 0.7517127394104197;
          } else {
            if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)59289.00000000000728) ) ) {
              result[0] += 0.7269823577447114;
            } else {
              if ( LIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)78836.50000000001455) ) ) {
                if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)97718.00000000001455) ) ) {
                  result[0] += 0.7446107384894015;
                } else {
                  result[0] += 0.7256209077662688;
                }
              } else {
                result[0] += 0.7543408190407485;
              }
            }
          }
        }
      }
    } else {
      result[0] += 0.7254596783218444;
    }
  }
  if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)51874.50000000000728) ) ) {
    if ( LIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)462.5000000000000568) ) ) {
      if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)42211.50000000000728) ) ) {
        result[0] += -0.03084476972288619;
      } else {
        result[0] += -0.0176392660750726;
      }
    } else {
      if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)35031.50000000000728) ) ) {
        if ( LIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)509.5000000000000568) ) ) {
          result[0] += -0.0392488305632882;
        } else {
          result[0] += -0.0046920688533645715;
        }
      } else {
        result[0] += -0.003721519837843884;
      }
    }
  } else {
    if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)434.5000000000000568) ) ) {
      if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)24.50000000000000355) ) ) {
        if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)79600.00000000001455) ) ) {
          if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)374.5000000000000568) ) ) {
            result[0] += -0.03312084897004774;
          } else {
            result[0] += -0.005322890710649787;
          }
        } else {
          if ( LIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)399.5000000000000568) ) ) {
            result[0] += -0.0009089427593692371;
          } else {
            result[0] += 0.012477988764203116;
          }
        }
      } else {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)348.5000000000000568) ) ) {
          if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)45.50000000000000711) ) ) {
            if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)217.5000000000000284) ) ) {
              result[0] += -0.01066022126468279;
            } else {
              result[0] += -0.02867961479412411;
            }
          } else {
            result[0] += -0.03753842360463272;
          }
        } else {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)201.5000000000000284) ) ) {
            result[0] += 0.0008512485619111357;
          } else {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)47.50000000000000711) ) ) {
              if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)83164.00000000001455) ) ) {
                result[0] += -0.015120055662231685;
              } else {
                if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)224726.0000000000291) ) ) {
                  result[0] += -0.0021979191684306223;
                } else {
                  result[0] += -0.018690891712433235;
                }
              }
            } else {
              result[0] += -0.030284241499136666;
            }
          }
        }
      }
    } else {
      if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)69515.00000000001455) ) ) {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)498.5000000000000568) ) ) {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)205.5000000000000284) ) ) {
            result[0] += 0.011060682891745761;
          } else {
            if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)22.50000000000000355) ) ) {
              result[0] += 0.004243548538047788;
            } else {
              result[0] += -0.010414359831597789;
            }
          }
        } else {
          if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)416.5000000000000568) ) ) {
            if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)33.50000000000000711) ) ) {
              result[0] += 0.010275456954282922;
            } else {
              if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)310.5000000000000568) ) ) {
                result[0] += 0.005699888261848155;
              } else {
                result[0] += -0.011515267745092482;
              }
            }
          } else {
            result[0] += -0.016188366041235542;
          }
        }
      } else {
        if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)190937.0000000000291) ) ) {
          if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)81420.50000000001455) ) ) {
            if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)491.5000000000000568) ) ) {
              result[0] += 0.002015249704509314;
            } else {
              result[0] += 0.009689219005797575;
            }
          } else {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)32.50000000000000711) ) ) {
              if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)449.5000000000000568) ) ) {
                result[0] += 0.005051168943564629;
              } else {
                result[0] += 0.016251047143193458;
              }
            } else {
              if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)225.5000000000000284) ) ) {
                result[0] += 0.021832500940806347;
              } else {
                result[0] += 0.005294937275753078;
              }
            }
          }
        } else {
          result[0] += -0.012199701481090576;
        }
      }
    }
  }
  if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)51874.50000000000728) ) ) {
    if ( LIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)488.5000000000000568) ) ) {
      if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)44068.00000000000728) ) ) {
        if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)55.50000000000000711) ) ) {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)176.5000000000000284) ) ) {
            result[0] += -0.003902066936140318;
          } else {
            if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)337.5000000000000568) ) ) {
              result[0] += -0.05004869389424412;
            } else {
              result[0] += -0.025964362375690213;
            }
          }
        } else {
          result[0] += -0.03590926361449794;
        }
      } else {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)348.5000000000000568) ) ) {
          result[0] += -0.03635915512081974;
        } else {
          result[0] += -0.012526368929142691;
        }
      }
    } else {
      if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)37004.00000000000728) ) ) {
        result[0] += -0.01806838876189142;
      } else {
        result[0] += 0.0006666835058954468;
      }
    }
  } else {
    if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)434.5000000000000568) ) ) {
      if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)24.50000000000000355) ) ) {
        if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)79600.00000000001455) ) ) {
          if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)374.5000000000000568) ) ) {
            result[0] += -0.032093895874217244;
          } else {
            result[0] += -0.0052066505448718926;
          }
        } else {
          if ( LIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)399.5000000000000568) ) ) {
            result[0] += -0.0008904743819273808;
          } else {
            result[0] += 0.012283917672736962;
          }
        }
      } else {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)348.5000000000000568) ) ) {
          result[0] += -0.0216599230285783;
        } else {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)201.5000000000000284) ) ) {
            result[0] += 0.0008344765386008673;
          } else {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)47.50000000000000711) ) ) {
              if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)83164.00000000001455) ) ) {
                result[0] += -0.014739708960428421;
              } else {
                if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)224726.0000000000291) ) ) {
                  result[0] += -0.0021522790356972676;
                } else {
                  result[0] += -0.018198677006287297;
                }
              }
            } else {
              result[0] += -0.02936971197868194;
            }
          }
        }
      }
    } else {
      if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)69515.00000000001455) ) ) {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)498.5000000000000568) ) ) {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)205.5000000000000284) ) ) {
            result[0] += 0.010882579778802635;
          } else {
            if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)22.50000000000000355) ) ) {
              result[0] += 0.004164974904781284;
            } else {
              result[0] += -0.010168770439373458;
            }
          }
        } else {
          if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)416.5000000000000568) ) ) {
            if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)33.50000000000000711) ) ) {
              result[0] += 0.010107354467742554;
            } else {
              result[0] += 0.0008132182137317947;
            }
          } else {
            result[0] += -0.015773943046412768;
          }
        }
      } else {
        if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)190937.0000000000291) ) ) {
          if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)81420.50000000001455) ) ) {
            if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)491.5000000000000568) ) ) {
              result[0] += 0.001976369704931933;
            } else {
              result[0] += 0.009528746582096136;
            }
          } else {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)32.50000000000000711) ) ) {
              if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)449.5000000000000568) ) ) {
                result[0] += 0.004959204305583905;
              } else {
                result[0] += 0.016021002728612045;
              }
            } else {
              if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)225.5000000000000284) ) ) {
                result[0] += 0.021567292231953687;
              } else {
                result[0] += 0.005198948648274931;
              }
            }
          }
        } else {
          result[0] += -0.011905005772969829;
        }
      }
    }
  }
  if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)51874.50000000000728) ) ) {
    if ( LIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)462.5000000000000568) ) ) {
      if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)42211.50000000000728) ) ) {
        result[0] += -0.02910215680417687;
      } else {
        result[0] += -0.016727524275270668;
      }
    } else {
      if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)35031.50000000000728) ) ) {
        if ( LIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)509.5000000000000568) ) ) {
          result[0] += -0.037224008013850826;
        } else {
          result[0] += -0.004202788807426084;
        }
      } else {
        result[0] += -0.003516043406783913;
      }
    }
  } else {
    if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)434.5000000000000568) ) ) {
      if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)24.50000000000000355) ) ) {
        if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)79600.00000000001455) ) ) {
          if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)374.5000000000000568) ) ) {
            result[0] += -0.031124328490154386;
          } else {
            result[0] += -0.005093219893579285;
          }
        } else {
          if ( LIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)399.5000000000000568) ) ) {
            result[0] += -0.0008723887472800099;
          } else {
            result[0] += 0.012092821945949059;
          }
        }
      } else {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)348.5000000000000568) ) ) {
          if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)45.50000000000000711) ) ) {
            if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)217.5000000000000284) ) ) {
              result[0] += -0.009900280213639921;
            } else {
              result[0] += -0.027199434032510465;
            }
          } else {
            result[0] += -0.035635436974674625;
          }
        } else {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)201.5000000000000284) ) ) {
            result[0] += 0.0008180306333047565;
          } else {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)47.50000000000000711) ) ) {
              if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)83164.00000000001455) ) ) {
                result[0] += -0.01437224719447103;
              } else {
                if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)224726.0000000000291) ) ) {
                  result[0] += -0.002107625852829132;
                } else {
                  result[0] += -0.01772510947456375;
                }
              }
            } else {
              result[0] += -0.02850298964996725;
            }
          }
        }
      }
    } else {
      if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)69515.00000000001455) ) ) {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)498.5000000000000568) ) ) {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)205.5000000000000284) ) ) {
            result[0] += 0.010707223024337676;
          } else {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)32.50000000000000711) ) ) {
              result[0] += 0.00013000864579489595;
            } else {
              result[0] += -0.013524556449553232;
            }
          }
        } else {
          if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)416.5000000000000568) ) ) {
            if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)33.50000000000000711) ) ) {
              result[0] += 0.009941849058794387;
            } else {
              if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)310.5000000000000568) ) ) {
                result[0] += 0.005582631803905574;
              } else {
                result[0] += -0.011258871238374749;
              }
            }
          } else {
            result[0] += -0.015374105809952672;
          }
        }
      } else {
        if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)190937.0000000000291) ) ) {
          if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)81420.50000000001455) ) ) {
            if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)491.5000000000000568) ) ) {
              result[0] += 0.0019382154257253658;
            } else {
              result[0] += 0.009370764949958185;
            }
          } else {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)35.50000000000000711) ) ) {
              if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)455.5000000000000568) ) ) {
                result[0] += 0.00669445806119195;
              } else {
                result[0] += 0.01566252428838093;
              }
            } else {
              if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)225.5000000000000284) ) ) {
                result[0] += 0.020254337565285913;
              } else {
                result[0] += 0.0038812232838518613;
              }
            }
          }
        } else {
          result[0] += -0.011619363940371871;
        }
      }
    }
  }
  if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)42402.50000000000728) ) ) {
    if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)20800.00000000000364) ) ) {
      if ( LIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)466.5000000000000568) ) ) {
        if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)23563.50000000000364) ) ) {
          result[0] += -0.022440619535795098;
        } else {
          if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)365.5000000000000568) ) ) {
            result[0] += -0.05324618277538088;
          } else {
            result[0] += -0.03305980135792198;
          }
        }
      } else {
        result[0] += -0.019722101028716892;
      }
    } else {
      if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)445.5000000000000568) ) ) {
        if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)273.5000000000000568) ) ) {
          if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)31842.50000000000364) ) ) {
            result[0] += 0.008051862353890101;
          } else {
            if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)31704.00000000000364) ) ) {
              result[0] += -0.03653180574987173;
            } else {
              if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)51359.00000000000728) ) ) {
                result[0] += -0.005426583294279641;
              } else {
                result[0] += -0.026859316441674354;
              }
            }
          }
        } else {
          if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)27939.00000000000364) ) ) {
            result[0] += -0.02382994416775838;
          } else {
            if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)66253.50000000001455) ) ) {
              result[0] += 0.00017631370930981313;
            } else {
              result[0] += -0.01694172514477506;
            }
          }
        }
      } else {
        if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)71475.00000000001455) ) ) {
          if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)384.5000000000000568) ) ) {
            if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)30881.50000000000364) ) ) {
              result[0] += -0.01679519249386237;
            } else {
              if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)345.5000000000000568) ) ) {
                if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)58397.00000000000728) ) ) {
                  result[0] += -0.004261904346361063;
                } else {
                  result[0] += -0.018212478116668814;
                }
              } else {
                result[0] += 0.006390417362831114;
              }
            }
          } else {
            result[0] += 0.006352976452344306;
          }
        } else {
          result[0] += -0.01970368642464628;
        }
      }
    }
  } else {
    if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)190937.0000000000291) ) ) {
      if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)53164.00000000000728) ) ) {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)462.5000000000000568) ) ) {
          if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)291.5000000000000568) ) ) {
            if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)195.5000000000000284) ) ) {
              result[0] += 8.109935617527535e-05;
            } else {
              result[0] += -0.014020821219435736;
            }
          } else {
            if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)106760.0000000000146) ) ) {
              result[0] += 0.0045325565500123725;
            } else {
              result[0] += -0.025515752333280312;
            }
          }
        } else {
          if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)323.5000000000000568) ) ) {
            if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)63193.50000000000728) ) ) {
              result[0] += 0.00985461381881215;
            } else {
              result[0] += -0.003032774861891466;
            }
          } else {
            result[0] += 0.011161401294612327;
          }
        }
      } else {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)434.5000000000000568) ) ) {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)250.5000000000000284) ) ) {
            if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)176.5000000000000284) ) ) {
              result[0] += 0.012103000619317794;
            } else {
              result[0] += -0.004904643614677864;
            }
          } else {
            result[0] += 0.01042600925076443;
          }
        } else {
          if ( LIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)72087.50000000001455) ) ) {
            if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)189.5000000000000284) ) ) {
              result[0] += -0.0025261267248114106;
            } else {
              result[0] += 0.013150727312314987;
            }
          } else {
            result[0] += 0.01848271009014669;
          }
        }
      }
    } else {
      result[0] += -0.01026927742583529;
    }
  }
  if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)41060.50000000000728) ) ) {
    if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)28996.50000000000364) ) ) {
      if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)364.5000000000000568) ) ) {
        if ( LIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)20800.00000000000364) ) ) {
          if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)23563.50000000000364) ) ) {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)53.50000000000000711) ) ) {
              if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)9925.000000000001819) ) ) {
                result[0] += -0.02961316495792998;
              } else {
                result[0] += 0.00182836525591965;
              }
            } else {
              result[0] += -0.03455983312527781;
            }
          } else {
            result[0] += -0.036729507452681436;
          }
        } else {
          result[0] += -0.022656442848069967;
        }
      } else {
        result[0] += -0.013395740140999814;
      }
    } else {
      if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)26.50000000000000355) ) ) {
        if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)58023.00000000000728) ) ) {
          result[0] += 0.013249950940728193;
        } else {
          result[0] += -0.00878960827812523;
        }
      } else {
        if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)58397.00000000000728) ) ) {
          if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)55.50000000000000711) ) ) {
            if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)48123.50000000000728) ) ) {
              result[0] += 0.002645327352332914;
            } else {
              if ( LIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)37607.50000000000728) ) ) {
                result[0] += -0.015950605401589407;
              } else {
                result[0] += 0.0005905260421661786;
              }
            }
          } else {
            result[0] += -0.0202848812938035;
          }
        } else {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)239.5000000000000284) ) ) {
            result[0] += -0.04777228550305486;
          } else {
            result[0] += -0.021678298315650792;
          }
        }
      }
    }
  } else {
    if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)190937.0000000000291) ) ) {
      if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)52735.00000000000728) ) ) {
        if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)35.50000000000000711) ) ) {
          if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)95373.50000000001455) ) ) {
            if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)323.5000000000000568) ) ) {
              result[0] += 0.0031689361428425744;
            } else {
              result[0] += 0.012710944790717358;
            }
          } else {
            result[0] += -0.011507704707088102;
          }
        } else {
          if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)63193.50000000000728) ) ) {
            if ( LIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)49937.00000000000728) ) ) {
              result[0] += -0.001980997341662661;
            } else {
              result[0] += 0.02377058610696027;
            }
          } else {
            result[0] += -0.014582865458938528;
          }
        }
      } else {
        if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)27.50000000000000355) ) ) {
          result[0] += 0.0166329538565652;
        } else {
          if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)68637.00000000001455) ) ) {
            result[0] += 0.022281755573991143;
          } else {
            if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)56980.00000000000728) ) ) {
              if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)52.50000000000000711) ) ) {
                result[0] += 0.002670584000031117;
              } else {
                result[0] += -0.02525499393124016;
              }
            } else {
              if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)80174.00000000001455) ) ) {
                if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)59.50000000000000711) ) ) {
                  result[0] += 0.017460620197276188;
                } else {
                  result[0] += -0.0052728352965233415;
                }
              } else {
                if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)68393.00000000001455) ) ) {
                  if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)89302.50000000001455) ) ) {
                    if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)80950.00000000001455) ) ) {
                      result[0] += -0.012335173359795362;
                    } else {
                      result[0] += 0.010297408190547155;
                    }
                  } else {
                    result[0] += -0.00747613180702917;
                  }
                } else {
                  result[0] += 0.012233893358291384;
                }
              }
            }
          }
        }
      }
    } else {
      result[0] += -0.010133472048313948;
    }
  }
  if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)61692.50000000000728) ) ) {
    if ( LIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)489.5000000000000568) ) ) {
      if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)44068.00000000000728) ) ) {
        if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)55.50000000000000711) ) ) {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)171.5000000000000284) ) ) {
            result[0] += -0.0008013516717503858;
          } else {
            result[0] += -0.024474292914897375;
          }
        } else {
          result[0] += -0.03259618680623706;
        }
      } else {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)376.5000000000000568) ) ) {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)167.5000000000000284) ) ) {
            result[0] += -0.009697546813527937;
          } else {
            result[0] += -0.03400900775343081;
          }
        } else {
          result[0] += -0.009295438113540274;
        }
      }
    } else {
      if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)31.50000000000000355) ) ) {
        if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)421.5000000000000568) ) ) {
          result[0] += 0.00839599564511728;
        } else {
          result[0] += -0.01779737208987425;
        }
      } else {
        if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)300.5000000000000568) ) ) {
          result[0] += 0.001643383553449699;
        } else {
          result[0] += -0.011218542549384647;
        }
      }
    }
  } else {
    if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)398.5000000000000568) ) ) {
      if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)70468.50000000001455) ) ) {
        result[0] += -0.023792613428372404;
      } else {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)356.5000000000000568) ) ) {
          result[0] += -0.013574852947335712;
        } else {
          result[0] += -0.0033960025354919706;
        }
      }
    } else {
      if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)85918.50000000001455) ) ) {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)472.5000000000000568) ) ) {
          if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)20.50000000000000355) ) ) {
            result[0] += 0.009015108751862764;
          } else {
            if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)241.5000000000000284) ) ) {
              result[0] += 0.003371995579887861;
            } else {
              result[0] += -0.006968776947846905;
            }
          }
        } else {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)272.5000000000000568) ) ) {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)46.50000000000000711) ) ) {
              result[0] += 0.015337262546228672;
            } else {
              result[0] += 0.003541887481984483;
            }
          } else {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)38.50000000000000711) ) ) {
              if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)353.5000000000000568) ) ) {
                if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)26.50000000000000355) ) ) {
                  result[0] += 0.01747863295300868;
                } else {
                  result[0] += 0.006602026141585386;
                }
              } else {
                if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)63498.00000000000728) ) ) {
                  result[0] += 0.022407018093274344;
                } else {
                  result[0] += 0.00016207615157809153;
                }
              }
            } else {
              if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)529.5000000000001137) ) ) {
                result[0] += -0.017991328405531117;
              } else {
                result[0] += 0.0031224541679947294;
              }
            }
          }
        }
      } else {
        if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)201930.5000000000291) ) ) {
          if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)27.50000000000000355) ) ) {
            if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)120839.0000000000146) ) ) {
              result[0] += 0.016365122866321868;
            } else {
              result[0] += 0.006502584794404569;
            }
          } else {
            if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)235.5000000000000284) ) ) {
              result[0] += 0.01828605107904969;
            } else {
              if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)456.5000000000000568) ) ) {
                if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)251.5000000000000284) ) ) {
                  result[0] += -0.03407871650482011;
                } else {
                  result[0] += -0.00022820164635448768;
                }
              } else {
                result[0] += 0.006938087484731265;
              }
            }
          }
        } else {
          result[0] += -0.012166838537068887;
        }
      }
    }
  }
  if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)41060.50000000000728) ) ) {
    if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)28996.50000000000364) ) ) {
      if ( LIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)487.5000000000000568) ) ) {
        if ( LIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)20800.00000000000364) ) ) {
          result[0] += -0.03150487278267267;
        } else {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)301.5000000000000568) ) ) {
            if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)171.5000000000000284) ) ) {
              result[0] += -0.014111133935906415;
            } else {
              if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)260.5000000000000568) ) ) {
                result[0] += -0.04371166145949851;
              } else {
                result[0] += -0.024830509058997526;
              }
            }
          } else {
            result[0] += -0.016377006264523646;
          }
        }
      } else {
        result[0] += -0.010281184665861245;
      }
    } else {
      if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)26.50000000000000355) ) ) {
        result[0] += -0.0016376435824567684;
      } else {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)374.5000000000000568) ) ) {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)163.5000000000000284) ) ) {
            result[0] += -0.008163072927012066;
          } else {
            result[0] += -0.032141031401353805;
          }
        } else {
          result[0] += -0.011653199194494743;
        }
      }
    }
  } else {
    if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)394.5000000000000568) ) ) {
      if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)25.50000000000000355) ) ) {
        result[0] += 0.0024618786262212426;
      } else {
        if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)184.5000000000000284) ) ) {
          if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)60.50000000000000711) ) ) {
            result[0] += 0.0051447344260268715;
          } else {
            result[0] += -0.021713312218064346;
          }
        } else {
          if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)52735.00000000000728) ) ) {
            result[0] += -0.024683262722643963;
          } else {
            if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)356.5000000000000568) ) ) {
              if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)216.5000000000000284) ) ) {
                result[0] += -0.005477212681339623;
              } else {
                result[0] += -0.02674378595601677;
              }
            } else {
              result[0] += 0.00027720741158165685;
            }
          }
        }
      }
    } else {
      if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)48659.00000000000728) ) ) {
        if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)38.50000000000000711) ) ) {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)322.5000000000000568) ) ) {
            result[0] += -0.0028919877671833827;
          } else {
            result[0] += 0.009240788191830822;
          }
        } else {
          result[0] += -0.007356646466082671;
        }
      } else {
        if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)30.50000000000000355) ) ) {
          if ( LIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)111512.0000000000146) ) ) {
            if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)21.50000000000000355) ) ) {
              result[0] += 0.018459450640127252;
            } else {
              result[0] += 0.013225616918198953;
            }
          } else {
            result[0] += -0.0013369173455994867;
          }
        } else {
          if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)434.5000000000000568) ) ) {
            if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)193.5000000000000284) ) ) {
              result[0] += 0.014342857066500276;
            } else {
              if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)50.50000000000000711) ) ) {
                if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)403.5000000000000568) ) ) {
                  result[0] += 0.01987296429915752;
                } else {
                  result[0] += -0.009889945762351277;
                }
              } else {
                result[0] += -0.03035768638037352;
              }
            }
          } else {
            if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)233.5000000000000284) ) ) {
              if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)47.50000000000000711) ) ) {
                result[0] += 0.018816956434109428;
              } else {
                result[0] += 0.005966114822840901;
              }
            } else {
              if ( LIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)550.5000000000001137) ) ) {
                result[0] += 0.00425955970525147;
              } else {
                result[0] += 0.012769123434177887;
              }
            }
          }
        }
      }
    }
  }
  if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)41060.50000000000728) ) ) {
    if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)28996.50000000000364) ) ) {
      if ( LIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)489.5000000000000568) ) ) {
        if ( LIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)20800.00000000000364) ) ) {
          if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)23563.50000000000364) ) ) {
            if ( LIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)12239.00000000000182) ) ) {
              result[0] += -0.030392069016939693;
            } else {
              if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)56.50000000000000711) ) ) {
                result[0] += 0.008357310990422247;
              } else {
                result[0] += -0.024237156812818367;
              }
            }
          } else {
            if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)365.5000000000000568) ) ) {
              result[0] += -0.04741023857478671;
            } else {
              result[0] += -0.029565028826653565;
            }
          }
        } else {
          if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)31842.50000000000364) ) ) {
            result[0] += 0.0007337974473727347;
          } else {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)35.50000000000000711) ) ) {
              if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)72882.50000000001455) ) ) {
                result[0] += -0.014263556151503428;
              } else {
                result[0] += -0.0334861313006302;
              }
            } else {
              result[0] += -0.0336807452680367;
            }
          }
        }
      } else {
        result[0] += -0.009729989616799075;
      }
    } else {
      if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)65387.00000000000728) ) ) {
        if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)31.50000000000000355) ) ) {
          if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)58023.00000000000728) ) ) {
            result[0] += 0.010938475763885177;
          } else {
            result[0] += -0.0033047766057891143;
          }
        } else {
          if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)52271.50000000000728) ) ) {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)55.50000000000000711) ) ) {
              result[0] += -0.0013095839801820146;
            } else {
              result[0] += -0.018160745642126372;
            }
          } else {
            result[0] += -0.020802616857243165;
          }
        }
      } else {
        if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)19.50000000000000355) ) ) {
          result[0] += -0.004694746509212084;
        } else {
          result[0] += -0.022313420862694226;
        }
      }
    }
  } else {
    if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)394.5000000000000568) ) ) {
      if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)25.50000000000000355) ) ) {
        result[0] += 0.002414728229769292;
      } else {
        result[0] += -0.007983737358993085;
      }
    } else {
      if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)48659.00000000000728) ) ) {
        if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)38.50000000000000711) ) ) {
          if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)462.5000000000000568) ) ) {
            if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)104746.5000000000146) ) ) {
              if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)21.50000000000000355) ) ) {
                result[0] += 0.013852409772999752;
              } else {
                result[0] += -0.006121101177970818;
              }
            } else {
              result[0] += -0.035280114912173165;
            }
          } else {
            result[0] += 0.007451016464313549;
          }
        } else {
          if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)61936.50000000000728) ) ) {
            result[0] += 0.0012069166167346427;
          } else {
            if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)43.50000000000000711) ) ) {
              result[0] += -0.03789813630816285;
            } else {
              result[0] += -0.01398922391593959;
            }
          }
        }
      } else {
        if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)201930.5000000000291) ) ) {
          if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)30.50000000000000355) ) ) {
            result[0] += 0.015863584198132816;
          } else {
            if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)434.5000000000000568) ) ) {
              result[0] += -0.0016877627354000137;
            } else {
              result[0] += 0.008564498938444364;
            }
          }
        } else {
          if ( UNLIKELY( !(data[1].missing != -1) || (data[1].fvalue <= (double)89456.00000000001455) ) ) {
            result[0] += -0.03715209535287208;
          } else {
            result[0] += -0.0066800615606574625;
          }
        }
      }
    }
  }
  if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)61692.50000000000728) ) ) {
    if ( LIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)489.5000000000000568) ) ) {
      if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)44068.00000000000728) ) ) {
        if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)55.50000000000000711) ) ) {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)171.5000000000000284) ) ) {
            result[0] += -0.00016048368554762607;
          } else {
            result[0] += -0.022497862045822707;
          }
        } else {
          result[0] += -0.03013952352387247;
        }
      } else {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)343.5000000000000568) ) ) {
          result[0] += -0.029120258085603826;
        } else {
          if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)68.50000000000001421) ) ) {
            if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)204.5000000000000284) ) ) {
              result[0] += 0.0019964609420989607;
            } else {
              if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)26.50000000000000355) ) ) {
                result[0] += -0.005401410916843842;
              } else {
                result[0] += -0.01586650777952578;
              }
            }
          } else {
            result[0] += -0.028665598694361992;
          }
        }
      }
    } else {
      if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)31.50000000000000355) ) ) {
        if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)421.5000000000000568) ) ) {
          result[0] += 0.008322455944906288;
        } else {
          result[0] += -0.01706040801902394;
        }
      } else {
        if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)300.5000000000000568) ) ) {
          result[0] += 0.0018208477519277763;
        } else {
          result[0] += -0.010545956136428445;
        }
      }
    }
  } else {
    if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)398.5000000000000568) ) ) {
      if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)70468.50000000001455) ) ) {
        result[0] += -0.0223723968650136;
      } else {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)356.5000000000000568) ) ) {
          result[0] += -0.012798806728634867;
        } else {
          result[0] += -0.0031639831927026923;
        }
      }
    } else {
      if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)85918.50000000001455) ) ) {
        if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)472.5000000000000568) ) ) {
          if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)20.50000000000000355) ) ) {
            result[0] += 0.008778918580973231;
          } else {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)52.50000000000000711) ) ) {
              result[0] += -0.002645651766510715;
            } else {
              result[0] += -0.01837006218293527;
            }
          }
        } else {
          if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)272.5000000000000568) ) ) {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)46.50000000000000711) ) ) {
              result[0] += 0.014857804610216444;
            } else {
              result[0] += 0.003277417159223568;
            }
          } else {
            if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)38.50000000000000711) ) ) {
              if ( LIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)353.5000000000000568) ) ) {
                if ( UNLIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)26.50000000000000355) ) ) {
                  result[0] += 0.016947407979535416;
                } else {
                  result[0] += 0.006220339291521315;
                }
              } else {
                if ( UNLIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)63498.00000000000728) ) ) {
                  result[0] += 0.022106996726348357;
                } else {
                  result[0] += -9.19453167972047e-05;
                }
              }
            } else {
              if ( UNLIKELY( !(data[2].missing != -1) || (data[2].fvalue <= (double)529.5000000000001137) ) ) {
                result[0] += -0.017515856594292504;
              } else {
                result[0] += 0.0029078571794988557;
              }
            }
          }
        }
      } else {
        if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)201930.5000000000291) ) ) {
          if ( LIKELY( !(data[3].missing != -1) || (data[3].fvalue <= (double)27.50000000000000355) ) ) {
            if ( LIKELY( !(data[0].missing != -1) || (data[0].fvalue <= (double)120839.0000000000146) ) ) {
              result[0] += 0.015777056084963267;
            } else {
              result[0] += 0.005949881649303852;
            }
          } else {
            if ( UNLIKELY( !(data[4].missing != -1) || (data[4].fvalue <= (double)235.5000000000000284) ) ) {
              result[0] += 0.017792477300357993;
            } else {
              result[0] += 0.003590058413947887;
            }
          }
        } else {
          result[0] += -0.011630408143238426;
        }
      }
    }
  }
  
  // Apply base_scores
  result[0] += 0;
  
  // Apply postprocessor
  if (!pred_margin) { postprocess(result); }
}

void postprocess(double* result) {
  // sigmoid
  const double alpha = (double)1;
  for (size_t i = 0; i < N_TARGET * MAX_N_CLASS; ++i) {
    result[i] = (double)(1) / ((double)(1) + exp(-alpha * result[i]));
  }
}

