/**
 * FILE:		pcmaudio.cpp
 * DESCRIPTION:
 *
 * This is the class for receiving and playing pcm audio
 * -----------------------------------------------------------------------
 *    Copyright (C) 2010-2015 OpenNetcam Project.
 *
 *   This  software is released under the following license:
 *        - GNU General Public License (GPL) version 3 for use with the
 *          Qt Open Source Edition (http://www.qt.io)
 *
 *    Permission to use, copy, modify, and distribute this software and its
 *    documentation for any purpose and without fee is hereby granted
 *    in accordance with the provisions of the GPLv3 which is available at:
 *    http://www.gnu.org/licenses/gpl.html
 *
 *    This software is provided "as is" without express or implied warranty.
 *
 * -----------------------------------------------------------------------
 */

#include <QtDebug>
#include <QDir>
#include "../include/common.h"
#include "vchannel.h"
#include "pcmaudio.h"
#include "avformat.h"
#include "aviformat.h"

using namespace command_line_arguments;

pcmAudio::pcmAudio( QObject *parent) : QObject( parent ), buffer(NULL), audio(NULL)
{
    QDEBUG << "pcmAudio";
    setFormat();
}

pcmAudio::~pcmAudio()
{
    QDEBUG << "~pcmAudio";
}

bool pcmAudio::ulaw2linear(const char *data, int len, AvFormat *av)
{
    if( buffer == NULL )
        return false;

    QByteArray bufferout;

#ifdef RECORD
        // save to file
        QString fileout = QDir::homePath()+QString("/" PROGRAM_NAME ".pcmu");

        QFile fout(fileout);
        if ( fout.open( QIODevice::WriteOnly | QIODevice::Append  ) )
        {
             QDataStream out(&fout);   // we will serialize the data into the file
             fout.write(data,len);   // serialize a string
             // out << "                ";
             fout.close();
        }
#endif


    quint16  t;
    const unsigned char * u_val = (const unsigned char*)data;
    unsigned char val;

    for (int xx=0; xx<len; xx++)
    {

     /* Complement to obtain normal u-law value. */
     //*u_val = ~*u_val;
     val = ~u_val[xx];

     /*
      * Extract and bias the quantization bits. Then
      * shift up by the segment number and subtract out the bias.
      */
     t = ((val & QUANT_MASK) << 3) + BIAS;
     t <<= ((unsigned)val & SEG_MASK) >> SEG_SHIFT;
     quint16 result = ((val & SIGN_BIT) ? (BIAS - t) : (t - BIAS));
     bufferout += (char)(result&0xff);
     bufferout += (char)(result>>8);

    }

    #ifdef RECORD
    {
        // save to file
        QString fileout = QDir::homePath()+QString("/" PROGRAM_NAME ".pcm");

        QFile fout(fileout);
        if ( fout.open( QIODevice::WriteOnly | QIODevice::Append  ) )
        {
             QDataStream out(&fout);   // we will serialize the data into the file
             fout.write(bufferout);
             // out << "                ";
             fout.close();
        }
    }
    #endif

    if( naudio == 2 )
        buffer->write( bufferout );

    if( av )
        av->recordFrame((const unsigned char*)bufferout.constData(), bufferout.count(), STREAMS_AUDIO);

    return true;
}



void pcmAudio::setFormat()
{
#ifdef QTMULTIMEDIA
    // this is pretty sure to be supported by everyone
    format.setFrequency(8000);
    format.setChannels(1);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::UnSignedInt);
    QDEBUG << "Set audio format " << format.codec();

    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    if (!info.isFormatSupported(format))
    {
        qWarning() << "raw audio format not supported by backend, cannot play audio.";
        return;
    }
    if( !audio )
    {
        audio = new QAudioOutput(format, this);
        if( audio )
            connect(audio,SIGNAL(stateChanged(QAudio::State)),SLOT(finishedPlaying(QAudio::State)));
    }
#endif
}

void pcmAudio::setData( QByteArray & data, AvFormat *av )
{
#ifdef QTMULTIMEDIA
    if( audio && audio->state() == QAudio::StoppedState )
    {
        QDEBUG << "Start audio output";
        buffer = audio->start();
    }
#endif
    if( buffer )
    {
        //buffer->open(QIODevice::Append);
        ulaw2linear( data.constData(), data.length(), av );
        //buffer->close();
    }
}

#ifdef QTMULTIMEDIA
void pcmAudio::finishedPlaying(QAudio::State state)
{
    QDEBUG << "Audio state=" << state;
}
#endif
