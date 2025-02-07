LUT_Size=32;

mprintf("const float LUT[PPP]={\n")
for n=1:LUT_Size
    mprintf("%.4ff",1+sin ((n-1)/LUT_Size*2*%pi));
    if n <> LUT_Size
        mprintf(",");
    end
    
    if modulo(n,5) == 0
        mprintf("\ \n");
    end
end
mprintf("};\n");

