void add_v(int q[10], int val, int i1)
{
    q[i1 % 10] = val;
    i1++;
    i1 %= 10;
}