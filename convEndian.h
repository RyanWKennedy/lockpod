#ifndef CONVENDIAN_H
#define CONVENDIAN_H

uint32_t be32dec(const void *pp);
void be32enc(void *pp, uint32_t x);
uint64_t be64dec(const void *pp);
void be64enc(void *pp, uint64_t x);
uint32_t le32dec(const void *pp);
void le32enc(void *pp, uint32_t x);
uint64_t le64dec(const void *pp);
void le64enc(void *pp, uint64_t x);

#endif