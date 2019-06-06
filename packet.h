
  struct packet{
    // char type; forgot about word alignment, so this would get padded
    unsigned int seq;
    unsigned int len;
    int payload; // first four bytes of payload
  };
