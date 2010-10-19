<?php

/**
 * Bulk parse
 */
function nomus_bulk_parse($nids){
	// no timeout
	set_time_limit(0);

	// sort the nodes by document source type (don't
	// hang on to the actual node due for memory considerations)
	$nodes = array();
	foreach($nids as $nid) {
		$node = node_load($nid, false, true);
		if($node->field_documentsource_ref[0]['nid']){
			$type = $node->field_documentsource_ref[0]['nid'];
			if(!isset($nodes[$type]))
				$nodes[$type] = array();
			$nodes[$type][] = $nid;
		}
	}
	// for each node type
	$nodesProcessed = 0;
	foreach($nodes as $nodeType => &$nodeGrouping){
		// initialize the parser for linktext
		if($docParserUserId = 
			nomus_documentparser_linktext_init(node_load($nodeGrouping[0], false, true),
				$node->field_documentsource_ref[0])) {
		
			// for each node	
			foreach($nodeGrouping as $nid){
				$node = node_load($nid, false, true);
				
				// parse
				$node = nomus_documentparser_linktext_run($node, $docParserUserId);
				node_save($node);
			}
		}
	
		// initialize the parser for parsing the fulltext
		if($docParserUserId = nomus_documentparser_fulltext_init(node_load($nodeGrouping[0], null, true),
			$node->field_documentsource_ref[0])) {
			// for each node
			foreach($nodeGrouping as $nid){
				$node = node_load($nid, null, true);
				
				$node = nomus_documentparser_fulltext_run($node, $docParserUserId);
				node_save($node);
			}
		}
	}
}


/**
 * Parse - run linktext and the document through
 * the configured GATE documents
 */
function nomus_documentparser_parse($docNode, $documentSourceNid) {
	global $user;
	if($docParserUserId = nomus_documentparser_linktext_init($docNode, $documentSourceNid)) {
		$docNode = nomus_documentparser_linktext_run($docNode, $docParserUserId);
	}
	if($docParserUserId = nomus_documentparser_fulltext_init($docNode, $documentSourceNid)) {
		$docNode = nomus_documentparser_fulltext_run($docNode, $docParserUserId);
	}
	return $docNode;
}
	
/**
 * Initialize the parser for parsing linktext
 * Returns true if there are linktext parsers to run.
 */
function nomus_documentparser_linktext_init($docNode, $documentSourceNid) {
	global $user;
	// document parser server
	$server = variable_get('nomus_documentparser_server', '');
	if($server == '') {
		return;
	}
	
	// get the document source node
	$docSourceNode = node_load($documentSourceNid);
	
	// generate a unique DocParser "user" from the userid and doctype
	// (it's better to create a new DocParser user for each
	// unique DocParser and never reset it, otherwise GATE leaks
	// the JAPE Parsers into the JVM each time the parser is
	// initialized)
	$docParserUserId = $user->uid . '-' .
		$documentSourceNid . '-linktext';
	
	// initialize
	if($docSourceNode->field_docparser_linktext_gate) {
	
		// query whether the parser exists
		$url = $server . '?req=parserExists';
		$url .= '&user=' . $docParserUserId;
		$request = new HttpRequest($url);
		$request->setMethod(HTTP_METH_GET);
		$result = $request->send();
		
		// if this parser has been initialized, nothing further to do
		if($result->getBody() == 'true') {
			return $docParserUserId;
		}
	
		// add each GATE document to the document parser
		foreach($docSourceNode->field_docparser_linktext_gate as $gateFileField) {
			$gateFileNid = $gateFileField['nid'];
			if($gateFileNid) {
				$gateFileNode = node_load($gateFileNid);
				$gateFileType = $gateFileNode->type;				
				
	   		// send a request to the server to add the document parser
				$url = $server . '?req=addApplication';
				$url .= '&user=' . $docParserUserId;
				$url .= '&fileUrl=' .
					urlencode(nomus_documentparser_gatedoc_url($gateFileNid));
				$url .= '&fileType=' . $gateFileType;
				$url .= '&debug=true';
				
				$request = new HttpRequest($url);
				$request->setMethod(HTTP_METH_GET);
				$result = $request->send();
			}
		}
		return $docParserUserId;
	}
	return false;
}

/**
 * Initialize the parser for parsing fulltext
 * Returns true if there are fulltext parsers to run.
 */
function nomus_documentparser_fulltext_init($docNode, $documentSourceNid) {
	global $user;
	
	// document parser server
	$server = variable_get('nomus_documentparser_server', '');
	if($server == '') {
		return;
	}
	
	// get the document source node
	$docSourceNode = node_load($documentSourceNid);

	// generate a unique DocParser "user" from the userid and doctype
	$docParserUserId = $user->uid . '-' .
		$documentSourceNid . '-fulltext';
		
	// initialize
	if($docSourceNode->field_docparser_fulltext_gate) {
	
		// query whether the parser exists
		$url = $server . '?req=parserExists';
		$url .= '&user=' . $docParserUserId;
		$request = new HttpRequest($url);
		$request->setMethod(HTTP_METH_GET);
		$result = $request->send();
		
		// if this parser has been initialized, nothing further to do
		if($result->getBody() == 'true') {
			return $docParserUserId;
		}
		
		
		// add each GATE document to the document parser
		foreach($docSourceNode->field_docparser_fulltext_gate as $gateFileField) {
			$gateFileNid = $gateFileField['nid'];
			if($gateFileNid) {
				$gateFileNode = node_load($gateFileNid);
				$gateFileType = $gateFileNode->type;				
				
	   		// send a request to the server to add the document parser
				$url = $server . '?req=addApplication';
				$url .= '&user=' . $docParserUserId;
				$url .= '&parserid=' . $docSourceNode->nid;
				$url .= '&fileUrl=' .
					urlencode(nomus_documentparser_gatedoc_url($gateFileNid));
				$url .= '&fileType=' . $gateFileType;
				$url .= '&debug=true';
				$request = new HttpRequest($url);
				$request->setMethod(HTTP_METH_GET);
				$result = $request->send();
			}
		}
		return $docParserUserId;
	}
	return false;
}

		
/**
 * Execute the linktext parsers
 */
function nomus_documentparser_linktext_run($docNode, $docParserUserId) {
	global $user;
				
	// document parser server
	$server = variable_get('nomus_documentparser_server', '');
	if($server == '') {
		return;
	}
	
	// convert linktext to utf-8
	$linktext = utf8_decode($docNode->field_raw_link_text[0]['value']);
	
	// parse the linktext
	$url = $server . '?req=run';
	$url .= '&user=' . $docParserUserId;
	$request = new HttpRequest($url);
	$request->setMethod(HTTP_METH_POST);
	$opts = array("Content-Type"=>"text/html; charset=UTF-8", "Accept"=>"text/plain");
	$request->addHeaders($opts);
	$request->setBody($linktext);
	$result = $request->send();
	$linkTextOutput = $result->getBody();

	// save the linktext xml data
	$xmlTagsStart = strpos($linkTextOutput, "<GateXMLTags>");
	$xmlTagsEnd = strpos($linkTextOutput, "</GateXMLTags>");
	if(!($xmlTagsStart === false) && !($xmlTagsEnd === false)) {
		$docNode->field_parsed_linktext_tags[0]['value'] =
		   '<xml>' .
			substr($linkTextOutput, $xmlTagsStart+13,
			($xmlTagsEnd-$xmlTagsStart)-13) .
			'</xml>';
	}
	return $docNode;
}

/**
 * Execute the linktext parsers
 */
function nomus_documentparser_fulltext_run($docNode, $docParserUserId) {
	global $user;
	
	// document parser server
	$server = variable_get('nomus_documentparser_server', '');
	if($server == '') {
		return;
	}
		
	// get the content
	$contentOutput = '';
	if($docNode->field_converted_file) {
		$htmlFilename = 
			$docNode->field_converted_file[0]['filepath'];
				
		// send the request
		$url = $server . '?req=run';
		$url .= '&user=' . $docParserUserId;
		$url .= '&parserid=' . $docNode->field_documentsource_ref[0]['nid'];
		$request = new HttpRequest($url);
		$request->setMethod(HTTP_METH_POST);
		$opts = array("Content-Type"=>"text/html", "Accept"=>"text/plain");
		$request->addHeaders($opts);
		$request->setBody(file_get_contents($htmlFilename));
		$result = $request->send();
		$contentOutput = $result->getBody();			
	}
		
	// if there's already a parsed HTML file, delete it
	if($docNode->field_parsed_html[0]) {
		$docNode->field_parsed_html = array();
		node_save($docNode);
	}
			
	// save the content xml data
	$xmlTagsStart = strpos($contentOutput, "<GateXMLTags>");
	$xmlTagsEnd = strpos($contentOutput, "</GateXMLTags>");
	if(!($xmlTagsStart === false) && !($xmlTagsEnd === false)) {

		$docNode->field_parsed_content_tags[0]['value'] =
		   '<xml>' .
			substr($contentOutput, $xmlTagsStart+13,
			($xmlTagsEnd-$xmlTagsStart)-13) .
			'</xml>';
	}
		
	// save the content html data
	$xmlTagsStart = strpos($contentOutput, "<GateHTML>");
	$xmlTagsEnd = strrpos($contentOutput, "</GateHTML>");
	if(!($xmlTagsStart === false) && !($xmlTagsEnd === false)) {

		$output = 
			substr($contentOutput, $xmlTagsStart+10,
			($xmlTagsEnd-$xmlTagsStart)-10);

		// keep the same filename, but with '_parsed'
		$pathInfo = pathinfo($docNode->field_raw_file[0]['filepath']);
		$tempFilename = $pathInfo['filename'] . '_parsed.html';
		$targetDir = $pathInfo['dirname'];		
		$tempFile = file_directory_temp() . '/' . $tempFilename;
		$fh = fopen($tempFile, "w");
		fwrite($fh, $output);
		fclose($fh);
			
		// save to the file field
		$docNode->field_parsed_html[0] = field_file_save_file(
  			$tempFile, array(), $targetDir);
	}
	return $docNode;
		
}

/**
 * Strips authentication tokens from urls, and translates
 * plugin directory urls to the local server
 *
 */
function nomus_documentparser_translate_gate_urls(&$node) {
	// strip tokens
	$node->body = preg_replace('/(<urlString>[^<]+token\=)[^<]*(<)/s', '$1$2', $node->body);
	
	// fix plugin directories
	$node->body = preg_replace('/(<urlString>)[^<]+\/plugins\/([^<]*<)/s', '$1$gatehome$plugins/$2', $node->body);
}

/**
 * Generates an external URL to retrieve a raw GATE document,
 * along with a user authentication token
 *
 */
function nomus_documentparser_gatedoc_url($gateDocNodeId) {
	return url('nomus/documentparser/gatedoc', array('absolute' => TRUE))
	   			. '?nodeId=' . $gateDocNodeId 
	   			. '&token=' . tokenauth_get_token();
}

/**
 * Menu callback to retrieve GATE documents
 */
function nomus_documentparser_gatedoc() {
	$nodeId = $_GET['nodeId'];
	$gateDoc = node_load($nodeId);
	
	// only retrieve GATE document types
	if($gateDoc && preg_match('/^gate_/', $gateDoc->type)) {
		$output = $gateDoc->body;
		
	   // special case for gazetteers
	   if($gateDoc->type == 'gate_gaz') {
	   	// if this is a def file request, generate the definition
	   	// list of the gazetteer lists
	   	if(!isset($_GET['lst'])) {
	   		$output = '';
	   	   foreach($gateDoc->field_gate_gazetteer as $key => $val) {
	   	   	$filename = "gatedoc?nodeId=$nodeId&lst=$key&token=" .
	   	   		tokenauth_get_token();
	   	   	$types = substr($val['value'], 0,
	   	   		strpos($val['value'], "\n"));
	   			$output .= $filename . ":" . $types . "\n";
	   		} 
	   	}
	   	// else, output the particular gazetteer list requested
	   	else {
	   		$lstId = $_GET['lst'];
	   		$gazField = $gateDoc->field_gate_gazetteer[$lstId]['value'];
	   		$output = substr($gazField, strpos($gazField, "\n")+1);
	   	}
	   }
	
		// special case for applications
		if($gateDoc->type == 'gate_gapp') {
			// replace the token document root with the document root of this server, and
			// token for this user
		   $output = preg_replace('/<urlString>http.*?nomus\/documentparser\/gatedoc\?(.*?token\=).*?<\/urlString>/s',
		   	'<urlString>' . url('nomus/documentparser/gatedoc', array('absolute' => TRUE)) . 
		   	'?${1}' . tokenauth_get_token() . '</urlString>', $output);
		   	/* $output = preg_replace('/<urlString>http.*?documentparser\/gatedoc(.*?token\=).*?<\/urlString>/s',
		   	'a', $output);*/

		}
		print $output;	
	}
}
?>