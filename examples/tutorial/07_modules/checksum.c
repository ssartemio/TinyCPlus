int status_checksum(int temperature, int load)
{
    return temperature * 31 + load;
}
