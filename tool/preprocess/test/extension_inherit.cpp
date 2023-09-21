INTERFACE:

// Variable number of class inheritance extensions

class Class_b0_e1
{
};

EXTENSION class Class_b0_e1 : Ext_base_0
{
};

// ----------------------------------------------------------------------
class Class_b0_e1_e1
{
};

EXTENSION class Class_b0_e1_e1 : Ext_base_0
{
};

EXTENSION class Class_b0_e1_e1 : Ext2_base_0
{
};

// ----------------------------------------------------------------------
class Class_b0_e2
{
};

EXTENSION class Class_b0_e2 : Ext_base_0, Ext_base_1
{
};

// ----------------------------------------------------------------------
class Class_b0_e2_e1
{
};

EXTENSION class Class_b0_e2_e1 : Ext_base_0, Ext_base_1
{
};

EXTENSION class Class_b0_e2_e1 : Ext2_base_0
{
};

// ----------------------------------------------------------------------
class Class_b1_e1 : Base_0
{
};

EXTENSION class Class_b1_e1 : Ext_base_0
{
};

// ----------------------------------------------------------------------
class Class_b1_e1_e1 : Base_0
{
};

EXTENSION class Class_b1_e1_e1 : Ext_base_0
{
};

EXTENSION class Class_b1_e1_e1 : Ext2_base_0
{
};

// ----------------------------------------------------------------------
class Class_b1_e2 : Base_0
{
};

EXTENSION class Class_b1_e2 : Ext_base_0, Ext_base_1
{
};

// ----------------------------------------------------------------------
class Class_b1_e2_e1 : Base_0
{
};

EXTENSION class Class_b1_e2_e1 : Ext_base_0, Ext_base_1
{
};

EXTENSION class Class_b1_e2_e1 : Ext2_base_0
{
};

// ----------------------------------------------------------------------
class Class_b2_e1 : Base_0, Base_1
{
};

EXTENSION class Class_b2_e1 : Ext_base_0
{
};

// ----------------------------------------------------------------------
class Class_b2_e1_e1 : Base_0, Base_1
{
};

EXTENSION class Class_b2_e1_e1 : Ext_base_0
{
};

EXTENSION class Class_b2_e1_e1 : Ext2_base_0
{
};

// ----------------------------------------------------------------------
class Class_b2_e2 : Base_0, Base_1
{
};

EXTENSION class Class_b2_e2 : Ext_base_0, Ext_base_1
{
};

// ----------------------------------------------------------------------
class Class_b2_e2_e1 : Base_0, Base_1
{
};

EXTENSION class Class_b2_e2_e1 : Ext_base_0, Ext_base_1
{
};

EXTENSION class Class_b2_e2_e1 : Ext2_base_0
{
};



// Variable spacing around class inheritance extensions

class Class_var : Base_0 /* Unconvenient comment
that wraps around */ {
};

EXTENSION class Class_var
: Ext_base_0
{
};

EXTENSION class Class_var : Ext2_base_0
{
};

EXTENSION class Class_var : Ext3_base_0 {};

IMPLEMENTATION:
