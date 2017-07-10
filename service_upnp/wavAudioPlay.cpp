
#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>

struct WAV_HEADER
{
    char rld[4];    //riff ��־����
    int rLen;       // www.it165.net
    char wld[4];    //��ʽ���ͣ�wave��
    char fld[4];    //"fmt"

    int fLen;   //sizeof(wave format matex)
    
    short wFormatTag;   //�����ʽ
    short wChannels;    //������
    int   nSamplesPersec ;  //����Ƶ��
    int   nAvgBitsPerSample;//WAVE�ļ�������С
    short  wBlockAlign; //�����
    short wBitsPerSample;   //WAVE�ļ�������С
    
    char dld[4];        //��data��
    int wSampleLength;  //��Ƶ���ݵĴ�С

} wav_header;


int set_pcm_play(FILE *fp);

int TestWavePlay(char* fileName)
{
    int nread;
    FILE *fp;
    fp=fopen(fileName,"rb");
    if(fp==NULL)
    {
        perror("open file failed:\n");
        return 0;
    }
    nread=fread(&wav_header,1,sizeof(wav_header),fp);
    set_pcm_play(fp);
	fclose(fp);
	printf("-- play %s finish\n", fileName);
    return 0;
}

int set_pcm_play(FILE *fp)
{
	int    rc;
	int    ret;
	int    size;
	snd_pcm_t*       handle;        //PCI�豸���
	snd_pcm_hw_params_t*      params;//Ӳ����Ϣ��PCM������
	unsigned int       val;
	int                dir=0;
	snd_pcm_uframes_t  frames;
	char   *buffer = 0;
	int channels=wav_header.wChannels;
	int frequency=wav_header.nSamplesPersec;
	int bit=wav_header.wBitsPerSample;
	int datablock=wav_header.wBlockAlign;
	unsigned char ch[100];  //�����洢wav�ļ���ͷ��Ϣ

	rc=snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if(rc<0)
	{
	        perror("\nopen PCM device failed:");
	        return rc;
	}

	snd_pcm_hw_params_alloca(&params); //����params�ṹ��
	if(rc<0)
	{
	        perror("\nsnd_pcm_hw_params_alloca:");
	       	goto errorExit;
	}
	 rc=snd_pcm_hw_params_any(handle, params);//��ʼ��params
	if(rc<0)
	{
	        perror("\nsnd_pcm_hw_params_any:");
			goto errorExit;
	}
	rc=snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);                                 //��ʼ������Ȩ��
	if(rc<0)
	{
	        perror("\nsed_pcm_hw_set_access:");
	        goto errorExit;
	}

	//����λ��
	switch(bit/8)
	{
		case 1:snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_U8);
			break ;
		case 2:snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
			break ;
		case 3:snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S24_LE);
			break ;
	}
	rc=snd_pcm_hw_params_set_channels(handle, params, channels);  //��������,1��ʾ����>����2��ʾ������
	if(rc<0)
	{
		perror("\nsnd_pcm_hw_params_set_channels:");
		goto errorExit;
	}
	val = frequency;
	rc=snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);  //����>Ƶ��
	if(rc<0)
	{
		perror("\nsnd_pcm_hw_params_set_rate_near:");
		goto errorExit;
	}

	rc = snd_pcm_hw_params(handle, params);
	if(rc<0)
	{
		perror("\nsnd_pcm_hw_params: ");
		goto errorExit;
	}

	rc= snd_pcm_hw_params_get_period_size(params, &frames, &dir);  /*��ȡ����
	����*/
	if(rc<0)
	{
			perror("\nsnd_pcm_hw_params_get_period_size:");
	        goto errorExit;
	}

	size = frames * datablock;   /*4 �������ݿ쳤��*/
	buffer =(char*)malloc(size);
	fseek(fp,58,SEEK_SET);  //��λ������������
	while (1)
	{
		memset(buffer,0,sizeof(buffer));
		ret = fread(buffer, 1, size, fp);
		if(ret == 0)
		{
		    printf("����д�����\n");
		    break;
		}
		else if (ret != size)
		{
		}
		// д��Ƶ���ݵ�PCM�豸  
		while(ret = snd_pcm_writei(handle, buffer, frames)<0)
		{
			usleep(2000);  
			if (ret == -EPIPE)
			{
				/* EPIPE means underrun */
				fprintf(stderr, "underrun occurred\n");
				//���Ӳ���������ã�ʹ�豸׼����  
				snd_pcm_prepare(handle);
			}
			else if (ret < 0)
			{
				fprintf(stderr, "error from writei: %s\n", snd_strerror(ret));
			}
		}
	}

	rc = 0;
	
errorExit:
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
	if(buffer)
		free(buffer);
	return rc;
}



//printf("nread=%d\n",nread);
//printf("RIFF ��־%s\n",wav_header.rld);
//printf("�ļ���СrLen��%d\n",wav_header.rLen);
//printf("wld=%s\n",wav_header.wld);
//printf("fld=%s\n",wav_header.fld);

// printf("fLen=%d\n",wav_header.fLen);

//printf("wFormatTag=%d\n",wav_header.wFormatTag);
//printf("��������%d\n",wav_header.wChannels);
//printf("����Ƶ�ʣ�%d\n",wav_header.nSamplesPersec);
//printf("nAvgBitsPerSample=%d\n",wav_header.nAvgBitsPerSample);
//printf("wBlockAlign=%d\n",wav_header.wBlockAlign);
//printf("������λ����%d\n",wav_header.wBitsPerSample);

// printf("data=%s\n",wav_header.dld);
//printf("wSampleLength=%d\n",wav_header.wSampleLength);

