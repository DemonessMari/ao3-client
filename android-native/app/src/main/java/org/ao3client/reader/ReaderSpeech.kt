package org.ao3client.reader

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.speech.tts.TextToSpeech
import android.speech.tts.UtteranceProgressListener
import androidx.compose.runtime.*
import java.util.Locale

internal fun speechChunkEnd(text:String,start:Int,limit:Int=350):Int {
    val cap=limit.coerceAtLeast(2)
    var end=minOf(text.length,start+cap)
    if(end<text.length && end>start && text[end-1].isHighSurrogate())end--
    if(end<text.length){val split=(end-1 downTo start+cap/2).firstOrNull {text[it] in "\n.!?。！？"};val space=split?:((end-1 downTo start+cap/2).firstOrNull{text[it].isWhitespace()});if(space!=null)end=space+1}
    return end
}
internal class ReaderSpeech(context:Context) {
    private val main=Handler(Looper.getMainLooper())
    private var engine:TextToSpeech?=null
    private var generation=0L
    private var utteranceId=""
    private var closed=false
    private val audio=context.applicationContext.getSystemService(Context.AUDIO_SERVICE) as android.media.AudioManager
    private val attributes=android.media.AudioAttributes.Builder().setUsage(android.media.AudioAttributes.USAGE_MEDIA).setContentType(android.media.AudioAttributes.CONTENT_TYPE_SPEECH).build()
    private val focus=android.media.AudioFocusRequest.Builder(android.media.AudioManager.AUDIOFOCUS_GAIN).setAudioAttributes(attributes).setOnAudioFocusChangeListener {change->main.post {if(change<0 && active && !paused)pauseOrResume()}}.build()
    private var text="";private var cursor=0;private var end=0;private var segmentStart=0
    var ready by mutableStateOf(false);private set
    var active by mutableStateOf(false);private set
    var paused by mutableStateOf(false);private set
    var error by mutableStateOf<String?>(null);private set
    init {
        engine=TextToSpeech(context.applicationContext){status->main.post {if(closed)return@post;ready=status==TextToSpeech.SUCCESS;if(!ready)error="The system speech engine is unavailable."}}
        engine?.setOnUtteranceProgressListener(object:UtteranceProgressListener(){
            override fun onStart(id:String?) {}
            override fun onDone(id:String?) {main.post {if(id==utteranceId && active && !paused){cursor=end;next()}}}
            override fun onError(id:String?) {main.post {if(id==utteranceId){active=false;paused=false;audio.abandonAudioFocusRequest(focus);error="The offline speech engine could not read this text."}}}
            override fun onRangeStart(id:String?,start:Int,finish:Int,frame:Int) {main.post {if(id==utteranceId && active && !paused)cursor=(segmentStart+start).coerceIn(segmentStart,end)}}
        })
    }
    fun start(content:String,offset:Int,language:String,rate:Float) {
        stop();error=null
        val tts=engine ?: return
        if(!ready){error="The speech engine is still starting.";return}
        val locale=Locale.forLanguageTag(language)
        val voice=tts.voices.orEmpty().filter {!it.isNetworkConnectionRequired && it.locale.language==locale.language}.sortedWith(compareByDescending<android.speech.tts.Voice>{it.locale.toLanguageTag().equals(locale.toLanguageTag(),true)}.thenBy{it.name}).firstOrNull()
        if(voice==null){error="Install an offline $language voice in the system text-to-speech settings.";return}
        if(tts.setVoice(voice)!=TextToSpeech.SUCCESS){error="Could not select the offline voice.";return}
        if(audio.requestAudioFocus(focus)!=android.media.AudioManager.AUDIOFOCUS_REQUEST_GRANTED){error="Audio output is busy. Try again when it is available.";return}
        tts.setAudioAttributes(attributes);tts.setSpeechRate(rate.coerceIn(.5f,2f));text=content;cursor=offset.coerceIn(0,text.length);active=true;next()
    }
    private fun next() {
        if(cursor>=text.length){active=false;paused=false;audio.abandonAudioFocusRequest(focus);return}
        segmentStart=cursor;end=speechChunkEnd(text,cursor);utteranceId="reader-$generation-$segmentStart"
        if(engine?.speak(text.substring(cursor,end),TextToSpeech.QUEUE_FLUSH,null,utteranceId)!=TextToSpeech.SUCCESS){active=false;audio.abandonAudioFocusRequest(focus);error="Could not start the offline voice."}
    }
    fun pauseOrResume() {if(!active)return;generation++;utteranceId=""
        if(paused){if(audio.requestAudioFocus(focus)!=android.media.AudioManager.AUDIOFOCUS_REQUEST_GRANTED){error="Audio output is busy.";return};paused=false;next()}
        else{paused=true;engine?.stop();audio.abandonAudioFocusRequest(focus)}}
    fun stop() {generation++;utteranceId="";active=false;paused=false;engine?.stop();audio.abandonAudioFocusRequest(focus)}
    fun close() {closed=true;stop();engine?.shutdown();engine=null;ready=false}
}
