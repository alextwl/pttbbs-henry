#!/usr/bin/perl
# $Id: FILTERMAIL.pm 2725 2005-05-16 18:36:27Z kcwu $
# ���d�ҶȨѰѦ�, �t�X util/filtermail.pl �ϥ�.
# �Ш̦ۦ�ݭn��g���m�� /home/bbs/bin �U.
# checkheader() �� checkbody()  �Ǧ^�����ɪ�ܪ����ᱼ�ӫʫH.
package FILTERMAIL;

sub checkheader
{
    return 0
	if( $_[0] =~ /^Subject: .*��P����/im ||
	    $_[0] =~ /^From: .*SpamCompany\.com/im );
    1;
}

sub checkbody
{
    return 0
	if( $_[0] =~ /<script language=\"JavaScript\"/im );
    1;
}

1;
