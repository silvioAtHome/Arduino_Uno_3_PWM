LUT_Size=24;

mprintf("const float LUT[PPP]={\n")
for n=1:LUT_Size
    mprintf("%.4ff", sin ((n-1)/LUT_Size*2*%pi));
    if n <> LUT_Size
        mprintf(",");
    end
    
    if modulo(n,5) == 0
        mprintf("\ \n");
    end
end
mprintf("};\n");

//p0=acc*2.0/((float) PPP);
//p0*=0.98;
//
printf("ppp:\t24\t32\n");
for acc = [1, 5, 50, 100]
    printf("acc:%i\t",acc);
    for ppp=[24, 32];
    printf("%f ",0.98 * acc*2.0/ppp);
    end
printf("\n");
end
