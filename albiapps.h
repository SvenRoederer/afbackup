#ifndef	__ALBIAPPS_H
#define	__ALBIAPPS_H	__ALBIAPPS_H

extern	int	albi_af_lockfile(int, char **);
extern	int	albi_bufrate(int, char **);
extern	int	albi_choice(int, char **);
extern	int	albi_getent(int, char **);
extern	int	albi_numset(int, char **);
extern	int	albi_rotor(int, char **);
extern	int	albi_ssu(int, char **);
extern	int	albi_ttywrap(int, char **);
extern	int	albi_udf(int, char **);
extern	int	albi_secbydate(int, char **);
extern	int	albi_datebysec(int, char **);
extern	int	albi_base64enc(int, char **);
extern	int	albi_base64dec(int, char **);
extern	int	albi_descrypt(int, char **);

extern	int	albi_main(int, char **);

#endif	/* defined(__ALBIAPPS_H) */
