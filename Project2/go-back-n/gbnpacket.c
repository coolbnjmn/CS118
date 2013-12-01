/* gbnpacket.c - defines the go-back-n packet structure
 * by Elijah Jordan Montgomery <elijah.montgomery@uky.edu>
 */
struct gbnpacket
{
  unsigned short int th_sport;
  unsigned short int th_dport;
  unsigned int th_seq;
  unsigned int th_ack;
  int length;
  char data[1006];
};
